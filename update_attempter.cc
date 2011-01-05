// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
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
#include "update_engine/update_check_scheduler.h"

using base::TimeDelta;
using base::TimeTicks;
using std::make_pair;
using std::tr1::shared_ptr;
using std::string;
using std::vector;

namespace chromeos_update_engine {

const int UpdateAttempter::kMaxDeltaUpdateFailures = 3;

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
      http_response_code_(0),
      priority_(utils::kProcessPriorityNormal),
      manage_priority_source_(NULL),
      download_active_(false),
      status_(UPDATE_STATUS_IDLE),
      download_progress_(0.0),
      last_checked_time_(0),
      new_version_("0.0.0.0"),
      new_size_(0),
      is_full_update_(false),
      proxy_manual_checks_(0),
      obeying_proxies_(true),
      chrome_proxy_resolver_(dbus_iface) {
  if (utils::FileExists(kUpdateCompletedMarker))
    status_ = UPDATE_STATUS_UPDATED_NEED_REBOOT;
}

UpdateAttempter::~UpdateAttempter() {
  CleanupPriorityManagement();
}

void UpdateAttempter::Update(const std::string& app_version,
                             const std::string& omaha_url,
                             bool obey_proxies) {
  if (status_ == UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    LOG(INFO) << "Not updating b/c we already updated and we're waiting for "
              << "reboot";
    return;
  }
  if (status_ != UPDATE_STATUS_IDLE) {
    // Update in progress. Do nothing
    return;
  }
  http_response_code_ = 0;
  if (!omaha_request_params_.Init(app_version, omaha_url)) {
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
  // Try harder to connect to the network. See comment in
  // libcurl_http_fetcher.cc.
  update_check_fetcher->set_no_network_max_retries(3);
  shared_ptr<OmahaRequestAction> update_check_action(
      new OmahaRequestAction(prefs_,
                             omaha_request_params_,
                             NULL,
                             update_check_fetcher));  // passes ownership
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
                             new LibcurlHttpFetcher(GetProxyResolver())));
  shared_ptr<DownloadAction> download_action(
      new DownloadAction(prefs_, new MultiRangeHTTPFetcher(
          new LibcurlHttpFetcher(GetProxyResolver()))));
  shared_ptr<OmahaRequestAction> download_finished_action(
      new OmahaRequestAction(prefs_,
                             omaha_request_params_,
                             new OmahaEvent(
                                 OmahaEvent::kTypeUpdateDownloadFinished),
                             new LibcurlHttpFetcher(GetProxyResolver())));
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
                             new LibcurlHttpFetcher(GetProxyResolver())));

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

  SetStatusAndNotify(UPDATE_STATUS_CHECKING_FOR_UPDATE);
  processor_->StartProcessing();
}

void UpdateAttempter::CheckForUpdate(const std::string& app_version,
                                     const std::string& omaha_url) {
  if (status_ != UPDATE_STATUS_IDLE) {
    LOG(INFO) << "Check for update requested, but status is "
              << UpdateStatusToString(status_) << ", so not checking.";
    return;
  }
  Update(app_version, omaha_url, true);
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
    SetStatusAndNotify(UPDATE_STATUS_IDLE);
    return;
  }

  if (code == kActionCodeSuccess) {
    utils::WriteFile(kUpdateCompletedMarker, "", 0);
    prefs_->SetInt64(kPrefsDeltaUpdateFailures, 0);
    prefs_->SetString(kPrefsPreviousVersion, omaha_request_params_.app_version);
    DeltaPerformer::ResetUpdateProgress(prefs_, false);
    SetStatusAndNotify(UPDATE_STATUS_UPDATED_NEED_REBOOT);

    // Report the time it took to update the system.
    int64_t update_time = time(NULL) - last_checked_time_;
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
  SetStatusAndNotify(UPDATE_STATUS_IDLE);
}

void UpdateAttempter::ProcessingStopped(const ActionProcessor* processor) {
  // Reset process priority back to normal.
  CleanupPriorityManagement();
  download_progress_ = 0.0;
  SetStatusAndNotify(UPDATE_STATUS_IDLE);
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
    // If this was a delta update attempt and the current state is at or past
    // the download phase, count the failure in case a switch to full update
    // becomes necessary. Ignore network transfer timeouts and failures.
    if (status_ >= UPDATE_STATUS_DOWNLOADING &&
        !is_full_update_ &&
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
    is_full_update_ = plan.is_full_update;
    SetupDownload();
    SetupPriorityManagement();
    SetStatusAndNotify(UPDATE_STATUS_UPDATE_AVAILABLE);
  } else if (type == DownloadAction::StaticType()) {
    SetStatusAndNotify(UPDATE_STATUS_FINALIZING);
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
    SetStatusAndNotify(UPDATE_STATUS_DOWNLOADING);
  }
}

bool UpdateAttempter::GetStatus(int64_t* last_checked_time,
                                double* progress,
                                std::string* current_operation,
                                std::string* new_version,
                                int64_t* new_size) {
  *last_checked_time = last_checked_time_;
  *progress = download_progress_;
  *current_operation = UpdateStatusToString(status_);
  *new_version = new_version_;
  *new_size = new_size_;
  return true;
}

void UpdateAttempter::SetStatusAndNotify(UpdateStatus status) {
  status_ = status;
  if (update_check_scheduler_) {
    update_check_scheduler_->SetUpdateStatus(status_);
  }
  if (!dbus_service_)
    return;
  last_notify_time_ = TimeTicks::Now();
  update_engine_service_emit_status_update(
      dbus_service_,
      last_checked_time_,
      download_progress_,
      UpdateStatusToString(status_),
      new_version_.c_str(),
      new_size_);
}

void UpdateAttempter::CreatePendingErrorEvent(AbstractAction* action,
                                              ActionExitCode code) {
  if (error_event_.get()) {
    // This shouldn't really happen.
    LOG(WARNING) << "There's already an existing pending error event.";
    return;
  }

  // For now assume that Omaha response action failure means that
  // there's no update so don't send an event. Also, double check that
  // the failure has not occurred while sending an error event -- in
  // which case don't schedule another. This shouldn't really happen
  // but just in case...
  if (action->Type() == OmahaResponseHandlerAction::StaticType() ||
      status_ == UPDATE_STATUS_REPORTING_ERROR_EVENT) {
    return;
  }

  code = GetErrorCodeForAction(action, code);
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
                             new LibcurlHttpFetcher(GetProxyResolver())));
  actions_.push_back(shared_ptr<AbstractAction>(error_event_action));
  processor_->EnqueueAction(error_event_action.get());
  SetStatusAndNotify(UPDATE_STATUS_REPORTING_ERROR_EVENT);
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
  CHECK(!is_full_update_);
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
  MultiRangeHTTPFetcher* fetcher =
      dynamic_cast<MultiRangeHTTPFetcher*>(download_action_->http_fetcher());
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
      fetcher->AddRange(resume_offset, -1);
    }
  } else {
    fetcher->AddRange(0, -1);
  }
}

}  // namespace chromeos_update_engine
