// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_attempter.h"

// From 'man clock_gettime': feature test macro: _POSIX_C_SOURCE >= 199309L
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif  // _POSIX_C_SOURCE
#include <time.h>

#include <string>
#include <tr1/memory>
#include <vector>

#include <base/rand_util.h>
#include <glib.h>
#include <metrics/metrics_library.h>
#include <policy/libpolicy.h>
#include <policy/device_policy.h>

#include "update_engine/certificate_checker.h"
#include "update_engine/dbus_service.h"
#include "update_engine/download_action.h"
#include "update_engine/filesystem_copier_action.h"
#include "update_engine/libcurl_http_fetcher.h"
#include "update_engine/multi_range_http_fetcher.h"
#include "update_engine/omaha_request_action.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/omaha_response_handler_action.h"
#include "update_engine/postinstall_runner_action.h"
#include "update_engine/prefs_interface.h"
#include "update_engine/subprocess.h"
#include "update_engine/update_check_scheduler.h"

using base::TimeDelta;
using base::TimeTicks;
using google::protobuf::NewPermanentCallback;
using std::make_pair;
using std::tr1::shared_ptr;
using std::string;
using std::vector;

namespace chromeos_update_engine {

const int UpdateAttempter::kMaxDeltaUpdateFailures = 3;

// Private test server URL w/ custom port number.
// TODO(garnold) This is a temporary hack to allow us to test the closed loop
// automated update testing. To be replaced with an hard-coded local IP address.
const char* const UpdateAttempter::kTestUpdateUrl(
    "http://garnold.mtv.corp.google.com:8080/update");

const char* kUpdateCompletedMarker =
    "/var/run/update_engine_autoupdate_completed";

namespace {
const int kMaxConsecutiveObeyProxyRequests = 20;
}  // namespace {}

const char* UpdateStatusToString(UpdateStatus status) {
  switch (status) {
    case UPDATE_STATUS_IDLE:
      return "UPDATE_STATUS_IDLE";
    case UPDATE_STATUS_CHECKING_FOR_UPDATE:
      return "UPDATE_STATUS_CHECKING_FOR_UPDATE";
    case UPDATE_STATUS_UPDATE_AVAILABLE:
      return "UPDATE_STATUS_UPDATE_AVAILABLE";
    case UPDATE_STATUS_DOWNLOADING:
      return "UPDATE_STATUS_DOWNLOADING";
    case UPDATE_STATUS_VERIFYING:
      return "UPDATE_STATUS_VERIFYING";
    case UPDATE_STATUS_FINALIZING:
      return "UPDATE_STATUS_FINALIZING";
    case UPDATE_STATUS_UPDATED_NEED_REBOOT:
      return "UPDATE_STATUS_UPDATED_NEED_REBOOT";
    case UPDATE_STATUS_REPORTING_ERROR_EVENT:
      return "UPDATE_STATUS_REPORTING_ERROR_EVENT";
    default:
      return "unknown status";
  }
}

// Turns a generic kActionCodeError to a generic error code specific
// to |action| (e.g., kActionCodeFilesystemCopierError). If |code| is
// not kActionCodeError, or the action is not matched, returns |code|
// unchanged.
ActionExitCode GetErrorCodeForAction(AbstractAction* action,
                                     ActionExitCode code) {
  if (code != kActionCodeError)
    return code;

  const string type = action->Type();
  if (type == OmahaRequestAction::StaticType())
    return kActionCodeOmahaRequestError;
  if (type == OmahaResponseHandlerAction::StaticType())
    return kActionCodeOmahaResponseHandlerError;
  if (type == FilesystemCopierAction::StaticType())
    return kActionCodeFilesystemCopierError;
  if (type == PostinstallRunnerAction::StaticType())
    return kActionCodePostinstallRunnerError;

  return code;
}

UpdateAttempter::UpdateAttempter(PrefsInterface* prefs,
                                 MetricsLibraryInterface* metrics_lib,
                                 DbusGlibInterface* dbus_iface)
    : processor_(new ActionProcessor()),
      dbus_service_(NULL),
      prefs_(prefs),
      metrics_lib_(metrics_lib),
      update_check_scheduler_(NULL),
      fake_update_success_(false),
      http_response_code_(0),
      priority_(utils::kProcessPriorityNormal),
      manage_priority_source_(NULL),
      download_active_(false),
      status_(UPDATE_STATUS_IDLE),
      download_progress_(0.0),
      last_checked_time_(0),
      new_version_("0.0.0.0"),
      new_size_(0),
      proxy_manual_checks_(0),
      obeying_proxies_(true),
      chrome_proxy_resolver_(dbus_iface),
      updated_boot_flags_(false),
      update_boot_flags_running_(false),
      start_action_processor_(false),
      policy_provider_(NULL),
      is_using_test_url_(false) {
  if (utils::FileExists(kUpdateCompletedMarker))
    status_ = UPDATE_STATUS_UPDATED_NEED_REBOOT;
}

UpdateAttempter::~UpdateAttempter() {
  CleanupPriorityManagement();
}

void UpdateAttempter::Update(const string& app_version,
                             const string& omaha_url,
                             bool obey_proxies,
                             bool interactive,
                             bool is_test) {
  chrome_proxy_resolver_.Init();
  fake_update_success_ = false;
  if (status_ == UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    // Although we have applied an update, we still want to ping Omaha
    // to ensure the number of active statistics is accurate.
    LOG(INFO) << "Not updating b/c we already updated and we're waiting for "
              << "reboot, we'll ping Omaha instead";
    PingOmaha();
    return;
  }
  if (status_ != UPDATE_STATUS_IDLE) {
    // Update in progress. Do nothing
    return;
  }
  http_response_code_ = 0;

  // Lazy initialize the policy provider, or reload the latest policy data.
  if (!policy_provider_.get()) {
    policy_provider_.reset(new policy::PolicyProvider());
  } else {
    policy_provider_->Reload();
  }

  // If the release_track is specified by policy, that takes precedence.
  string release_track;
  if (policy_provider_->device_policy_is_loaded())
    policy_provider_->GetDevicePolicy().GetReleaseChannel(&release_track);

  // Determine whether an alternative test address should be used.
  string omaha_url_to_use = omaha_url;
  if ((is_using_test_url_ = (omaha_url_to_use.empty() && is_test))) {
    omaha_url_to_use = kTestUpdateUrl;
    LOG(INFO) << "using alternative server address: " << omaha_url_to_use;
  }

  if (!omaha_request_params_.Init(app_version, omaha_url_to_use,
                                  release_track)) {
    LOG(ERROR) << "Unable to initialize Omaha request device params.";
    return;
  }

  obeying_proxies_ = true;
  if (obey_proxies || proxy_manual_checks_ == 0) {
    LOG(INFO) << "forced to obey proxies";
    // If forced to obey proxies, every 20th request will not use proxies
    proxy_manual_checks_++;
    LOG(INFO) << "proxy manual checks: " << proxy_manual_checks_;
    if (proxy_manual_checks_ >= kMaxConsecutiveObeyProxyRequests) {
      proxy_manual_checks_ = 0;
      obeying_proxies_ = false;
    }
  } else if (base::RandInt(0, 4) == 0) {
    obeying_proxies_ = false;
  }
  LOG_IF(INFO, !obeying_proxies_) << "To help ensure updates work, this update "
      "check we are ignoring the proxy settings and using "
      "direct connections.";

  DisableDeltaUpdateIfNeeded();
  CHECK(!processor_->IsRunning());
  processor_->set_delegate(this);

  // Actions:
  LibcurlHttpFetcher* update_check_fetcher =
      new LibcurlHttpFetcher(GetProxyResolver());
  // Try harder to connect to the network, esp when not interactive.
  // See comment in libcurl_http_fetcher.cc.
  update_check_fetcher->set_no_network_max_retries(interactive ? 1 : 3);
  update_check_fetcher->set_check_certificate(CertificateChecker::kUpdate);
  shared_ptr<OmahaRequestAction> update_check_action(
      new OmahaRequestAction(prefs_,
                             omaha_request_params_,
                             NULL,
                             update_check_fetcher,  // passes ownership
                             false));
  shared_ptr<OmahaResponseHandlerAction> response_handler_action(
      new OmahaResponseHandlerAction(prefs_));
  shared_ptr<FilesystemCopierAction> filesystem_copier_action(
      new FilesystemCopierAction(false, false));
  shared_ptr<FilesystemCopierAction> kernel_filesystem_copier_action(
      new FilesystemCopierAction(true, false));
  shared_ptr<OmahaRequestAction> download_started_action(
      new OmahaRequestAction(prefs_,
                             omaha_request_params_,
                             new OmahaEvent(
                                 OmahaEvent::kTypeUpdateDownloadStarted),
                             new LibcurlHttpFetcher(GetProxyResolver()),
                             false));
  LibcurlHttpFetcher* download_fetcher =
      new LibcurlHttpFetcher(GetProxyResolver());
  download_fetcher->set_check_certificate(CertificateChecker::kDownload);
  shared_ptr<DownloadAction> download_action(
      new DownloadAction(prefs_,
                         new MultiRangeHttpFetcher(
                             download_fetcher)));  // passes ownership
  shared_ptr<OmahaRequestAction> download_finished_action(
      new OmahaRequestAction(prefs_,
                             omaha_request_params_,
                             new OmahaEvent(
                                 OmahaEvent::kTypeUpdateDownloadFinished),
                             new LibcurlHttpFetcher(GetProxyResolver()),
                             false));
  shared_ptr<FilesystemCopierAction> filesystem_verifier_action(
      new FilesystemCopierAction(false, true));
  shared_ptr<FilesystemCopierAction> kernel_filesystem_verifier_action(
      new FilesystemCopierAction(true, true));
  shared_ptr<PostinstallRunnerAction> postinstall_runner_action(
      new PostinstallRunnerAction);
  shared_ptr<OmahaRequestAction> update_complete_action(
      new OmahaRequestAction(prefs_,
                             omaha_request_params_,
                             new OmahaEvent(OmahaEvent::kTypeUpdateComplete),
                             new LibcurlHttpFetcher(GetProxyResolver()),
                             false));

  download_action->set_delegate(this);
  response_handler_action_ = response_handler_action;
  download_action_ = download_action;

  actions_.push_back(shared_ptr<AbstractAction>(update_check_action));
  actions_.push_back(shared_ptr<AbstractAction>(response_handler_action));
  actions_.push_back(shared_ptr<AbstractAction>(filesystem_copier_action));
  actions_.push_back(shared_ptr<AbstractAction>(
      kernel_filesystem_copier_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_started_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_finished_action));
  actions_.push_back(shared_ptr<AbstractAction>(filesystem_verifier_action));
  actions_.push_back(shared_ptr<AbstractAction>(
      kernel_filesystem_verifier_action));
  actions_.push_back(shared_ptr<AbstractAction>(postinstall_runner_action));
  actions_.push_back(shared_ptr<AbstractAction>(update_complete_action));

  // Enqueue the actions
  for (vector<shared_ptr<AbstractAction> >::iterator it = actions_.begin();
       it != actions_.end(); ++it) {
    processor_->EnqueueAction(it->get());
  }

  // Bond them together. We have to use the leaf-types when calling
  // BondActions().
  BondActions(update_check_action.get(),
              response_handler_action.get());
  BondActions(response_handler_action.get(),
              filesystem_copier_action.get());
  BondActions(filesystem_copier_action.get(),
              kernel_filesystem_copier_action.get());
  BondActions(kernel_filesystem_copier_action.get(),
              download_action.get());
  BondActions(download_action.get(),
              filesystem_verifier_action.get());
  BondActions(filesystem_verifier_action.get(),
              kernel_filesystem_verifier_action.get());
  BondActions(kernel_filesystem_verifier_action.get(),
              postinstall_runner_action.get());

  SetStatusAndNotify(UPDATE_STATUS_CHECKING_FOR_UPDATE,
                     kUpdateNoticeUnspecified);

  // Just in case we didn't update boot flags yet, make sure they're updated
  // before any update processing starts.
  start_action_processor_ = true;
  UpdateBootFlags();
}

void UpdateAttempter::CheckForUpdate(const string& app_version,
                                     const string& omaha_url) {
  if (status_ != UPDATE_STATUS_IDLE) {
    LOG(INFO) << "Check for update requested, but status is "
              << UpdateStatusToString(status_) << ", so not checking.";
    return;
  }

  // Read GPIO signals and determine whether this is an automated test scenario.
  // For safety, we only allow a test update to be performed once; subsequent
  // update requests will be carried out normally.
  static bool is_test_used_once = false;
  bool is_test = !is_test_used_once && GpioHandler::IsGpioSignalingTest();
  if (is_test) {
    LOG(INFO) << "test mode signaled";
    is_test_used_once = true;
  }

  Update(app_version, omaha_url, true, true, is_test);
}

bool UpdateAttempter::RebootIfNeeded() {
  if (status_ != UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    LOG(INFO) << "Reboot requested, but status is "
              << UpdateStatusToString(status_) << ", so not rebooting.";
    return false;
  }
  TEST_AND_RETURN_FALSE(utils::Reboot());
  return true;
}

// Delegate methods:
void UpdateAttempter::ProcessingDone(const ActionProcessor* processor,
                                     ActionExitCode code) {
  CHECK(response_handler_action_);
  LOG(INFO) << "Processing Done.";
  actions_.clear();

  // Reset process priority back to normal.
  CleanupPriorityManagement();

  if (status_ == UPDATE_STATUS_REPORTING_ERROR_EVENT) {
    LOG(INFO) << "Error event sent.";

    // Inform scheduler of new status; also specifically inform about a failed
    // update attempt with a test address.
    SetStatusAndNotify(UPDATE_STATUS_IDLE,
                       (is_using_test_url_ ? kUpdateNoticeTestAddrFailed :
                        kUpdateNoticeUnspecified));

    if (!fake_update_success_) {
      return;
    }
    LOG(INFO) << "Booted from FW B and tried to install new firmware, "
        "so requesting reboot from user.";
  }

  if (code == kActionCodeSuccess) {
    utils::WriteFile(kUpdateCompletedMarker, "", 0);
    prefs_->SetInt64(kPrefsDeltaUpdateFailures, 0);
    prefs_->SetString(kPrefsPreviousVersion, omaha_request_params_.app_version);
    DeltaPerformer::ResetUpdateProgress(prefs_, false);
    SetStatusAndNotify(UPDATE_STATUS_UPDATED_NEED_REBOOT,
                       kUpdateNoticeUnspecified);

    // Report the time it took to update the system.
    int64_t update_time = time(NULL) - last_checked_time_;
    if (!fake_update_success_)
      metrics_lib_->SendToUMA("Installer.UpdateTime",
                              static_cast<int>(update_time),  // sample
                              1,  // min = 1 second
                              20 * 60,  // max = 20 minutes
                              50);  // buckets
    return;
  }

  if (ScheduleErrorEventAction()) {
    return;
  }
  LOG(INFO) << "No update.";
  SetStatusAndNotify(UPDATE_STATUS_IDLE, kUpdateNoticeUnspecified);
}

void UpdateAttempter::ProcessingStopped(const ActionProcessor* processor) {
  // Reset process priority back to normal.
  CleanupPriorityManagement();
  download_progress_ = 0.0;
  SetStatusAndNotify(UPDATE_STATUS_IDLE, kUpdateNoticeUnspecified);
  actions_.clear();
  error_event_.reset(NULL);
}

// Called whenever an action has finished processing, either successfully
// or otherwise.
void UpdateAttempter::ActionCompleted(ActionProcessor* processor,
                                      AbstractAction* action,
                                      ActionExitCode code) {
  // Reset download progress regardless of whether or not the download
  // action succeeded. Also, get the response code from HTTP request
  // actions (update download as well as the initial update check
  // actions).
  const string type = action->Type();
  if (type == DownloadAction::StaticType()) {
    download_progress_ = 0.0;
    DownloadAction* download_action = dynamic_cast<DownloadAction*>(action);
    http_response_code_ = download_action->GetHTTPResponseCode();
  } else if (type == OmahaRequestAction::StaticType()) {
    OmahaRequestAction* omaha_request_action =
        dynamic_cast<OmahaRequestAction*>(action);
    // If the request is not an event, then it's the update-check.
    if (!omaha_request_action->IsEvent()) {
      http_response_code_ = omaha_request_action->GetHTTPResponseCode();
      // Forward the server-dictated poll interval to the update check
      // scheduler, if any.
      if (update_check_scheduler_) {
        update_check_scheduler_->set_poll_interval(
            omaha_request_action->GetOutputObject().poll_interval);
      }
    }
  }
  if (code != kActionCodeSuccess) {
    // If the current state is at or past the download phase, count the failure
    // in case a switch to full update becomes necessary. Ignore network
    // transfer timeouts and failures.
    if (status_ >= UPDATE_STATUS_DOWNLOADING &&
        code != kActionCodeDownloadTransferError) {
      MarkDeltaUpdateFailure();
    }
    // On failure, schedule an error event to be sent to Omaha.
    CreatePendingErrorEvent(action, code);
    return;
  }
  // Find out which action completed.
  if (type == OmahaResponseHandlerAction::StaticType()) {
    // Note that the status will be updated to DOWNLOADING when some bytes get
    // actually downloaded from the server and the BytesReceived callback is
    // invoked. This avoids notifying the user that a download has started in
    // cases when the server and the client are unable to initiate the download.
    CHECK(action == response_handler_action_.get());
    const InstallPlan& plan = response_handler_action_->install_plan();
    last_checked_time_ = time(NULL);
    // TODO(adlr): put version in InstallPlan
    new_version_ = "0.0.0.0";
    new_size_ = plan.size;
    SetupDownload();
    SetupPriorityManagement();
    SetStatusAndNotify(UPDATE_STATUS_UPDATE_AVAILABLE,
                       kUpdateNoticeUnspecified);
  } else if (type == DownloadAction::StaticType()) {
    SetStatusAndNotify(UPDATE_STATUS_FINALIZING, kUpdateNoticeUnspecified);
  }
}

// Stop updating. An attempt will be made to record status to the disk
// so that updates can be resumed later.
void UpdateAttempter::Terminate() {
  // TODO(adlr): implement this method.
  NOTIMPLEMENTED();
}

// Try to resume from a previously Terminate()d update.
void UpdateAttempter::ResumeUpdating() {
  // TODO(adlr): implement this method.
  NOTIMPLEMENTED();
}

void UpdateAttempter::SetDownloadStatus(bool active) {
  download_active_ = active;
  LOG(INFO) << "Download status: " << (active ? "active" : "inactive");
}

void UpdateAttempter::BytesReceived(uint64_t bytes_received, uint64_t total) {
  if (!download_active_) {
    LOG(ERROR) << "BytesReceived called while not downloading.";
    return;
  }
  double progress = static_cast<double>(bytes_received) /
      static_cast<double>(total);
  // Self throttle based on progress. Also send notifications if
  // progress is too slow.
  const double kDeltaPercent = 0.01;  // 1%
  if (status_ != UPDATE_STATUS_DOWNLOADING ||
      bytes_received == total ||
      progress - download_progress_ >= kDeltaPercent ||
      TimeTicks::Now() - last_notify_time_ >= TimeDelta::FromSeconds(10)) {
    download_progress_ = progress;
    SetStatusAndNotify(UPDATE_STATUS_DOWNLOADING, kUpdateNoticeUnspecified);
  }
}

bool UpdateAttempter::GetStatus(int64_t* last_checked_time,
                                double* progress,
                                string* current_operation,
                                string* new_version,
                                int64_t* new_size) {
  *last_checked_time = last_checked_time_;
  *progress = download_progress_;
  *current_operation = UpdateStatusToString(status_);
  *new_version = new_version_;
  *new_size = new_size_;
  return true;
}

void UpdateAttempter::UpdateBootFlags() {
  if (update_boot_flags_running_) {
    LOG(INFO) << "Update boot flags running, nothing to do.";
    return;
  }
  if (updated_boot_flags_) {
    LOG(INFO) << "Already updated boot flags. Skipping.";
    if (start_action_processor_) {
      ScheduleProcessingStart();
    }
    return;
  }
  // This is purely best effort. Failures should be logged by Subprocess. Run
  // the script asynchronously to avoid blocking the event loop regardless of
  // the script runtime.
  update_boot_flags_running_ = true;
  LOG(INFO) << "Updating boot flags...";
  vector<string> cmd(1, "/usr/sbin/chromeos-setgoodkernel");
  if (!Subprocess::Get().Exec(cmd, StaticCompleteUpdateBootFlags, this)) {
    CompleteUpdateBootFlags(1);
  }
}

void UpdateAttempter::CompleteUpdateBootFlags(int return_code) {
  update_boot_flags_running_ = false;
  updated_boot_flags_ = true;
  if (start_action_processor_) {
    ScheduleProcessingStart();
  }
}

void UpdateAttempter::StaticCompleteUpdateBootFlags(
    int return_code,
    const string& output,
    void* p) {
  reinterpret_cast<UpdateAttempter*>(p)->CompleteUpdateBootFlags(return_code);
}

void UpdateAttempter::BroadcastStatus() {
  if (!dbus_service_) {
    return;
  }
  last_notify_time_ = TimeTicks::Now();
  update_engine_service_emit_status_update(
      dbus_service_,
      last_checked_time_,
      download_progress_,
      UpdateStatusToString(status_),
      new_version_.c_str(),
      new_size_);
}

void UpdateAttempter::SetStatusAndNotify(UpdateStatus status,
                                         UpdateNotice notice) {
  status_ = status;
  if (update_check_scheduler_) {
    update_check_scheduler_->SetUpdateStatus(status_, notice);
  }
  BroadcastStatus();
}

void UpdateAttempter::CreatePendingErrorEvent(AbstractAction* action,
                                              ActionExitCode code) {
  if (error_event_.get()) {
    // This shouldn't really happen.
    LOG(WARNING) << "There's already an existing pending error event.";
    return;
  }

  // For now assume that a generic Omaha response action failure means that
  // there's no update so don't send an event. Also, double check that the
  // failure has not occurred while sending an error event -- in which case
  // don't schedule another. This shouldn't really happen but just in case...
  if ((action->Type() == OmahaResponseHandlerAction::StaticType() &&
       code == kActionCodeError) ||
      status_ == UPDATE_STATUS_REPORTING_ERROR_EVENT) {
    return;
  }

  code = GetErrorCodeForAction(action, code);
  fake_update_success_ = code == kActionCodePostinstallBootedFromFirmwareB;

  // Apply the bit modifiers to the error code.
  if (!utils::IsNormalBootMode()) {
    code = static_cast<ActionExitCode>(code | kActionCodeBootModeFlag);
  }
  if (response_handler_action_.get() &&
      response_handler_action_->install_plan().is_resume) {
    code = static_cast<ActionExitCode>(code | kActionCodeResumedFlag);
  }
  error_event_.reset(new OmahaEvent(OmahaEvent::kTypeUpdateComplete,
                                    OmahaEvent::kResultError,
                                    code));
}

bool UpdateAttempter::ScheduleErrorEventAction() {
  if (error_event_.get() == NULL)
    return false;

  LOG(INFO) << "Update failed -- reporting the error event.";
  shared_ptr<OmahaRequestAction> error_event_action(
      new OmahaRequestAction(prefs_,
                             omaha_request_params_,
                             error_event_.release(),  // Pass ownership.
                             new LibcurlHttpFetcher(GetProxyResolver()),
                             false));
  actions_.push_back(shared_ptr<AbstractAction>(error_event_action));
  processor_->EnqueueAction(error_event_action.get());
  SetStatusAndNotify(UPDATE_STATUS_REPORTING_ERROR_EVENT,
                     kUpdateNoticeUnspecified);
  processor_->StartProcessing();
  return true;
}

void UpdateAttempter::SetPriority(utils::ProcessPriority priority) {
  if (priority_ == priority) {
    return;
  }
  if (utils::SetProcessPriority(priority)) {
    priority_ = priority;
    LOG(INFO) << "Process priority = " << priority_;
  }
}

void UpdateAttempter::SetupPriorityManagement() {
  if (manage_priority_source_) {
    LOG(ERROR) << "Process priority timeout source hasn't been destroyed.";
    CleanupPriorityManagement();
  }
  const int kPriorityTimeout = 2 * 60 * 60;  // 2 hours
  manage_priority_source_ = g_timeout_source_new_seconds(kPriorityTimeout);
  g_source_set_callback(manage_priority_source_,
                        StaticManagePriorityCallback,
                        this,
                        NULL);
  g_source_attach(manage_priority_source_, NULL);
  SetPriority(utils::kProcessPriorityLow);
}

void UpdateAttempter::CleanupPriorityManagement() {
  if (manage_priority_source_) {
    g_source_destroy(manage_priority_source_);
    manage_priority_source_ = NULL;
  }
  SetPriority(utils::kProcessPriorityNormal);
}

gboolean UpdateAttempter::StaticManagePriorityCallback(gpointer data) {
  return reinterpret_cast<UpdateAttempter*>(data)->ManagePriorityCallback();
}

gboolean UpdateAttempter::StaticStartProcessing(gpointer data) {
  reinterpret_cast<UpdateAttempter*>(data)->processor_->StartProcessing();
  return FALSE;  // Don't call this callback again.
}

void UpdateAttempter::ScheduleProcessingStart() {
  LOG(INFO) << "Scheduling an action processor start.";
  start_action_processor_ = false;
  g_idle_add(&StaticStartProcessing, this);
}

bool UpdateAttempter::ManagePriorityCallback() {
  SetPriority(utils::kProcessPriorityNormal);
  manage_priority_source_ = NULL;
  return false;  // Destroy the timeout source.
}

void UpdateAttempter::DisableDeltaUpdateIfNeeded() {
  int64_t delta_failures;
  if (omaha_request_params_.delta_okay &&
      prefs_->GetInt64(kPrefsDeltaUpdateFailures, &delta_failures) &&
      delta_failures >= kMaxDeltaUpdateFailures) {
    LOG(WARNING) << "Too many delta update failures, forcing full update.";
    omaha_request_params_.delta_okay = false;
  }
}

void UpdateAttempter::MarkDeltaUpdateFailure() {
  // Don't try to resume a failed delta update.
  DeltaPerformer::ResetUpdateProgress(prefs_, false);
  int64_t delta_failures;
  if (!prefs_->GetInt64(kPrefsDeltaUpdateFailures, &delta_failures) ||
      delta_failures < 0) {
    delta_failures = 0;
  }
  prefs_->SetInt64(kPrefsDeltaUpdateFailures, ++delta_failures);
}

void UpdateAttempter::SetupDownload() {
  MultiRangeHttpFetcher* fetcher =
      dynamic_cast<MultiRangeHttpFetcher*>(download_action_->http_fetcher());
  fetcher->ClearRanges();
  if (response_handler_action_->install_plan().is_resume) {
    // Resuming an update so fetch the update manifest metadata first.
    int64_t manifest_metadata_size = 0;
    prefs_->GetInt64(kPrefsManifestMetadataSize, &manifest_metadata_size);
    fetcher->AddRange(0, manifest_metadata_size);
    // If there're remaining unprocessed data blobs, fetch them. Be careful not
    // to request data beyond the end of the payload to avoid 416 HTTP response
    // error codes.
    int64_t next_data_offset = 0;
    prefs_->GetInt64(kPrefsUpdateStateNextDataOffset, &next_data_offset);
    uint64_t resume_offset = manifest_metadata_size + next_data_offset;
    if (resume_offset < response_handler_action_->install_plan().size) {
      fetcher->AddRange(resume_offset);
    }
  } else {
    fetcher->AddRange(0);
  }
}

void UpdateAttempter::PingOmaha() {
  if (!processor_->IsRunning()) {
    shared_ptr<OmahaRequestAction> ping_action(
        new OmahaRequestAction(prefs_,
                               omaha_request_params_,
                               NULL,
                               new LibcurlHttpFetcher(GetProxyResolver()),
                               true));
    actions_.push_back(shared_ptr<OmahaRequestAction>(ping_action));
    processor_->set_delegate(NULL);
    processor_->EnqueueAction(ping_action.get());
    // Call StartProcessing() synchronously here to avoid any race conditions
    // caused by multiple outstanding ping Omaha requests.  If we call
    // StartProcessing() asynchronously, the device can be suspended before we
    // get a chance to callback to StartProcessing().  When the device resumes
    // (assuming the device sleeps longer than the next update check period),
    // StartProcessing() is called back and at the same time, the next update
    // check is fired which eventually invokes StartProcessing().  A crash
    // can occur because StartProcessing() checks to make sure that the
    // processor is idle which it isn't due to the two concurrent ping Omaha
    // requests.
    processor_->StartProcessing();
  } else {
    LOG(WARNING) << "Action processor running, Omaha ping suppressed.";
  }

  // Update the status which will schedule the next update check
  SetStatusAndNotify(UPDATE_STATUS_UPDATED_NEED_REBOOT,
                     kUpdateNoticeUnspecified);
}

}  // namespace chromeos_update_engine
