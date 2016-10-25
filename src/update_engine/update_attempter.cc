// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_attempter.h"

// From 'man clock_gettime': feature test macro: _POSIX_C_SOURCE >= 199309L
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif  // _POSIX_C_SOURCE
#include <time.h>

#include <string>
#include <memory>
#include <vector>

#include <glib.h>

#include "files/file_util.h"
#include "update_engine/certificate_checker.h"
#include "update_engine/dbus_service.h"
#include "update_engine/download_action.h"
#include "update_engine/filesystem_copier_action.h"
#include "update_engine/kernel_copier_action.h"
#include "update_engine/kernel_verifier_action.h"
#include "update_engine/libcurl_http_fetcher.h"
#include "update_engine/multi_range_http_fetcher.h"
#include "update_engine/omaha_request_action.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/omaha_response_handler_action.h"
#include "update_engine/payload_state_interface.h"
#include "update_engine/pcr_policy_post_action.h"
#include "update_engine/postinstall_runner_action.h"
#include "update_engine/prefs_interface.h"
#include "update_engine/subprocess.h"
#include "update_engine/system_state.h"
#include "update_engine/update_check_scheduler.h"

using google::protobuf::NewPermanentCallback;
using std::chrono::steady_clock;
using std::make_pair;
using std::shared_ptr;
using std::set;
using std::string;
using std::vector;

namespace chromeos_update_engine {

const int UpdateAttempter::kMaxDeltaUpdateFailures = 3;

const char* kUpdateCompletedMarker =
    "/var/run/update_engine_autoupdate_completed";

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

UpdateAttempter::UpdateAttempter(SystemState* system_state,
                                 DbusGlibInterface* dbus_iface)
    : processor_(new ActionProcessor()),
      system_state_(system_state),
      dbus_service_(NULL),
      update_check_scheduler_(NULL),
      fake_update_success_(false),
      http_response_code_(0),
      download_active_(false),
      status_(UPDATE_STATUS_IDLE),
      download_progress_(0.0),
      last_checked_time_(0),
      new_version_("0.0.0.0"),
      new_payload_size_(0),
      updated_boot_flags_(false),
      update_boot_flags_running_(false),
      start_action_processor_(false) {
  prefs_ = system_state->prefs();
  omaha_request_params_ = system_state->request_params();
  if (utils::FileExists(kUpdateCompletedMarker)){
    status_ = UPDATE_STATUS_UPDATED_NEED_REBOOT;
    updated_boot_flags_(true);
  }
}

void UpdateAttempter::Update(bool interactive) {
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

  if (!CalculateUpdateParams(interactive)) {
    return;
  }

  BuildUpdateActions(interactive);

  SetStatusAndNotify(UPDATE_STATUS_CHECKING_FOR_UPDATE,
                     kUpdateNoticeUnspecified);

  // Just in case we didn't update boot flags yet, make sure they're updated
  // before any update processing starts.
  start_action_processor_ = true;
  UpdateBootFlags();
}

bool UpdateAttempter::CalculateUpdateParams(bool interactive) {
  http_response_code_ = 0;

  if (!omaha_request_params_->Init(interactive)) {
    LOG(ERROR) << "Unable to initialize Omaha request device params.";
    return false;
  }

  DisableDeltaUpdateIfNeeded();
  return true;
}

void UpdateAttempter::BuildUpdateActions(bool interactive) {
  CHECK(!processor_->IsRunning());
  processor_->set_delegate(this);

  // Actions:
  LibcurlHttpFetcher* update_check_fetcher = new LibcurlHttpFetcher();
  // Try harder to connect to the network, esp when not interactive.
  // See comment in libcurl_http_fetcher.cc.
  update_check_fetcher->set_no_network_max_retries(interactive ? 1 : 3);
  update_check_fetcher->set_check_certificate(CertificateChecker::kUpdate);
  shared_ptr<OmahaRequestAction> update_check_action(
      new OmahaRequestAction(system_state_,
                             NULL,
                             update_check_fetcher,  // passes ownership
                             false));
  shared_ptr<OmahaResponseHandlerAction> response_handler_action(
      new OmahaResponseHandlerAction(system_state_));
  shared_ptr<FilesystemCopierAction> filesystem_copier_action(
      new FilesystemCopierAction(false));
  shared_ptr<KernelCopierAction> kernel_copier_action(new KernelCopierAction);
  shared_ptr<OmahaRequestAction> download_started_action(
      new OmahaRequestAction(system_state_,
                             new OmahaEvent(
                                 OmahaEvent::kTypeUpdateDownloadStarted),
                             new LibcurlHttpFetcher(),
                             false));
  LibcurlHttpFetcher* download_fetcher = new LibcurlHttpFetcher();
  download_fetcher->set_check_certificate(CertificateChecker::kDownload);
  shared_ptr<DownloadAction> download_action(
      new DownloadAction(prefs_,
                         new MultiRangeHttpFetcher(
                             download_fetcher)));  // passes ownership
  shared_ptr<OmahaRequestAction> download_finished_action(
      new OmahaRequestAction(system_state_,
                             new OmahaEvent(
                                 OmahaEvent::kTypeUpdateDownloadFinished),
                             new LibcurlHttpFetcher(),
                             false));
  shared_ptr<FilesystemCopierAction> filesystem_verifier_action(
      new FilesystemCopierAction(true));
  shared_ptr<KernelVerifierAction> kernel_verifier_action(
      new KernelVerifierAction);
  shared_ptr<PCRPolicyPostAction> pcr_policy_post_action(
      new PCRPolicyPostAction(system_state_, new LibcurlHttpFetcher()));
  shared_ptr<PostinstallRunnerAction> postinstall_runner_action(
      new PostinstallRunnerAction);
  shared_ptr<OmahaRequestAction> update_complete_action(
      new OmahaRequestAction(system_state_,
                             new OmahaEvent(OmahaEvent::kTypeUpdateComplete),
                             new LibcurlHttpFetcher(),
                             false));

  download_action->set_delegate(this);
  response_handler_action_ = response_handler_action;
  download_action_ = download_action;

  actions_.push_back(shared_ptr<AbstractAction>(update_check_action));
  actions_.push_back(shared_ptr<AbstractAction>(response_handler_action));
  actions_.push_back(shared_ptr<AbstractAction>(filesystem_copier_action));
  actions_.push_back(shared_ptr<AbstractAction>(kernel_copier_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_started_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_finished_action));
  actions_.push_back(shared_ptr<AbstractAction>(filesystem_verifier_action));
  actions_.push_back(shared_ptr<AbstractAction>(kernel_verifier_action));
  actions_.push_back(shared_ptr<AbstractAction>(pcr_policy_post_action));
  actions_.push_back(shared_ptr<AbstractAction>(postinstall_runner_action));
  actions_.push_back(shared_ptr<AbstractAction>(update_complete_action));

  // Enqueue the actions
  for (const shared_ptr<AbstractAction>& action : actions_) {
    processor_->EnqueueAction(action.get());
  }

  // Bond them together. We have to use the leaf-types when calling
  // BondActions().
  BondActions(update_check_action.get(),
              response_handler_action.get());
  BondActions(response_handler_action.get(),
              filesystem_copier_action.get());
  BondActions(filesystem_copier_action.get(),
              kernel_copier_action.get());
  BondActions(kernel_copier_action.get(),
              download_action.get());
  BondActions(download_action.get(),
              filesystem_verifier_action.get());
  BondActions(filesystem_verifier_action.get(),
              kernel_verifier_action.get());
  BondActions(kernel_verifier_action.get(),
              pcr_policy_post_action.get());
  BondActions(pcr_policy_post_action.get(),
              postinstall_runner_action.get());
}

void UpdateAttempter::CheckForUpdate(bool interactive) {
  LOG(INFO) << "New update check requested";

  if (status_ != UPDATE_STATUS_IDLE) {
    LOG(INFO) << "Skipping update check because current status is "
              << UpdateStatusToString(status_);
    return;
  }

  // Pass through the interactive flag, in case we want to simulate a scheduled
  // test.
  Update(interactive);
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

  if (status_ == UPDATE_STATUS_REPORTING_ERROR_EVENT) {
    LOG(INFO) << "Error event sent.";

    // Inform scheduler of new status
    SetStatusAndNotify(UPDATE_STATUS_IDLE, kUpdateNoticeUnspecified);

    if (!fake_update_success_) {
      return;
    }
    LOG(INFO) << "Booted from FW B and tried to install new firmware, "
        "so requesting reboot from user.";
  }

  if (code == kActionCodeSuccess) {
    utils::WriteFile(kUpdateCompletedMarker, "", 0);
    prefs_->SetInt64(kPrefsDeltaUpdateFailures, 0);
    prefs_->SetString(kPrefsPreviousVersion,
                      omaha_request_params_->app_version());
    PayloadProcessor::ResetUpdateProgress(prefs_, false);

    LOG(INFO) << "Update successfully applied, waiting to reboot.";

    SetStatusAndNotify(UPDATE_STATUS_UPDATED_NEED_REBOOT,
                       kUpdateNoticeUnspecified);
    return;
  }

  if (ScheduleErrorEventAction()) {
    return;
  }
  LOG(INFO) << "No update.";
  SetStatusAndNotify(UPDATE_STATUS_IDLE, kUpdateNoticeUnspecified);
}

void UpdateAttempter::ProcessingStopped(const ActionProcessor* processor) {
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
    new_payload_size_ = plan.payload_size;
    SetupDownload();
    SetStatusAndNotify(UPDATE_STATUS_UPDATE_AVAILABLE,
                       kUpdateNoticeUnspecified);
  } else if (type == DownloadAction::StaticType()) {
    // At this point, we are guaranteed to have downloaded a full payload, i.e
    // the one whose size matches the size mentioned in Omaha response. If any
    // errors happen after this, it's likely a problem with the payload itself or
    // the state of the system and not a problem with the URL or network.  So,
    // indicate that to the payload state so that AU can backoff appropriately.
    system_state_->payload_state()->DownloadComplete();
    SetStatusAndNotify(UPDATE_STATUS_FINALIZING, kUpdateNoticeUnspecified);
  }
}

void UpdateAttempter::SetDownloadStatus(bool active) {
  download_active_ = active;
  LOG(INFO) << "Download status: " << (active ? "active" : "inactive");
}

void UpdateAttempter::BytesReceived(uint64_t received,
                                    uint64_t progress,
                                    uint64_t total) {
  if (!download_active_) {
    LOG(ERROR) << "BytesReceived called while not downloading.";
    return;
  }

  system_state_->payload_state()->DownloadProgress(received);

  double pct = static_cast<double>(progress) / static_cast<double>(total);
  // Self throttle based on progress. Also send notifications if
  // progress is too slow.
  const double kDeltaPercent = 0.01;  // 1%
  if (status_ != UPDATE_STATUS_DOWNLOADING ||
      progress == total ||
      pct - download_progress_ >= kDeltaPercent ||
      steady_clock::now() - last_notify_time_ >= std::chrono::seconds(10)) {
    download_progress_ = pct;
    LOG(INFO) << "Downloaded " << progress << "/" << total
              << " bytes (" << static_cast<int>(100.0*pct) << "%)";
    SetStatusAndNotify(UPDATE_STATUS_DOWNLOADING, kUpdateNoticeUnspecified);
  }
}

bool UpdateAttempter::ResetStatus() {
  LOG(INFO) << "Attempting to reset state from "
            << UpdateStatusToString(status_) << " to UPDATE_STATUS_IDLE";

  switch (status_) {
    case UPDATE_STATUS_IDLE:
      // no-op.
      return true;

    case UPDATE_STATUS_UPDATED_NEED_REBOOT:  {
      status_ = UPDATE_STATUS_IDLE;
      LOG(INFO) << "Reset Successful";

      // also remove the reboot marker so that if the machine is rebooted
      // after resetting to idle state, it doesn't go back to
      // UPDATE_STATUS_UPDATED_NEED_REBOOT state.
      const files::FilePath kUpdateCompletedMarkerPath(kUpdateCompletedMarker);
      return files::DeleteFile(kUpdateCompletedMarkerPath, false);
    }

    default:
      LOG(ERROR) << "Reset not allowed in this state.";
      return false;
  }
}

bool UpdateAttempter::GetStatus(int64_t* last_checked_time,
                                double* progress,
                                string* current_operation,
                                string* new_version,
                                int64_t* new_payload_size) {
  *last_checked_time = last_checked_time_;
  *progress = download_progress_;
  *current_operation = UpdateStatusToString(status_);
  *new_version = new_version_;
  *new_payload_size = new_payload_size_;
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
  vector<string> cmd{set_good_partition_cmd_};
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
  last_notify_time_ = steady_clock::now();
  update_engine_service_emit_status_update(
      dbus_service_,
      last_checked_time_,
      download_progress_,
      UpdateStatusToString(status_),
      new_version_.c_str(),
      new_payload_size_);
}

uint32_t UpdateAttempter::GetErrorCodeFlags()  {
  uint32_t flags = 0;

  if (response_handler_action_.get() &&
      response_handler_action_->install_plan().is_resume)
    flags |= kActionCodeResumedFlag;

  if (!utils::IsOfficialBuild())
    flags |= kActionCodeTestImageFlag;

  if (omaha_request_params_->update_url() != kProductionOmahaUrl)
    flags |= kActionCodeTestOmahaUrlFlag;

  return flags;
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

  // Classify the code to generate the appropriate result so that
  // the Borgmon charts show up the results correctly.
  // Do this before calling GetErrorCodeForAction which could potentially
  // augment the bit representation of code and thus cause no matches for
  // the switch cases below.
  OmahaEvent::Result event_result;
  switch (code) {
    case kActionCodeOmahaUpdateIgnoredPerPolicy:
    case kActionCodeOmahaUpdateDeferredPerPolicy:
    case kActionCodeOmahaUpdateDeferredForBackoff:
      event_result = OmahaEvent::kResultUpdateDeferred;
      break;
    default:
      event_result = OmahaEvent::kResultError;
      break;
  }

  code = GetErrorCodeForAction(action, code);
  fake_update_success_ = code == kActionCodePostinstallBootedFromFirmwareB;

  // Compute the final error code with all the bit flags to be sent to Omaha.
  code = static_cast<ActionExitCode>(code | GetErrorCodeFlags());
  error_event_.reset(new OmahaEvent(OmahaEvent::kTypeUpdateComplete,
                                    event_result,
                                    code));
}

bool UpdateAttempter::ScheduleErrorEventAction() {
  if (error_event_.get() == NULL)
    return false;

  LOG(ERROR) << "Update failed.";
  system_state_->payload_state()->UpdateFailed(error_event_->error_code);

  // Send it to Omaha.
  shared_ptr<OmahaRequestAction> error_event_action(
      new OmahaRequestAction(system_state_,
                             error_event_.release(),  // Pass ownership.
                             new LibcurlHttpFetcher(),
                             false));
  actions_.push_back(shared_ptr<AbstractAction>(error_event_action));
  processor_->EnqueueAction(error_event_action.get());
  SetStatusAndNotify(UPDATE_STATUS_REPORTING_ERROR_EVENT,
                     kUpdateNoticeUnspecified);
  processor_->StartProcessing();
  return true;
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

void UpdateAttempter::DisableDeltaUpdateIfNeeded() {
  int64_t delta_failures;
  if (omaha_request_params_->delta_okay() &&
      prefs_->GetInt64(kPrefsDeltaUpdateFailures, &delta_failures) &&
      delta_failures >= kMaxDeltaUpdateFailures) {
    LOG(WARNING) << "Too many delta update failures, forcing full update.";
    omaha_request_params_->set_delta_okay(false);
  }
}

void UpdateAttempter::MarkDeltaUpdateFailure() {
  // Don't try to resume a failed delta update.
  PayloadProcessor::ResetUpdateProgress(prefs_, false);
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
    if (resume_offset < response_handler_action_->install_plan().payload_size) {
      fetcher->AddRange(resume_offset);
    }
  } else {
    fetcher->AddRange(0);
  }
}

void UpdateAttempter::PingOmaha() {
  if (!processor_->IsRunning()) {
    shared_ptr<OmahaRequestAction> ping_action(
        new OmahaRequestAction(system_state_,
                               NULL,
                               new LibcurlHttpFetcher(),
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
