// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_attempter.h"

// From 'man clock_gettime': feature test macro: _POSIX_C_SOURCE >= 199309L
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif  // _POSIX_C_SOURCE
#include <time.h>

#include <tr1/memory>
#include <string>
#include <vector>

#include <glib.h>

#include "metrics/metrics_library.h"
#include "update_engine/dbus_service.h"
#include "update_engine/download_action.h"
#include "update_engine/filesystem_copier_action.h"
#include "update_engine/libcurl_http_fetcher.h"
#include "update_engine/omaha_request_action.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/omaha_response_handler_action.h"
#include "update_engine/postinstall_runner_action.h"
#include "update_engine/set_bootable_flag_action.h"

using std::tr1::shared_ptr;
using std::string;
using std::vector;

namespace chromeos_update_engine {

const char* kUpdateCompletedMarker = "/tmp/update_engine_autoupdate_completed";

namespace {
// Returns true on success.
bool GetCPUClockTime(struct timespec* out) {
  return clock_gettime(CLOCK_REALTIME, out) == 0;
}
// Returns stop - start.
struct timespec CPUClockTimeElapsed(const struct timespec& start,
                                    const struct timespec& stop) {
  CHECK(start.tv_sec >= 0);
  CHECK(stop.tv_sec >= 0);
  CHECK(start.tv_nsec >= 0);
  CHECK(stop.tv_nsec >= 0);

  const int64_t kOneBillion = 1000000000L;
  const int64_t start64 = start.tv_sec * kOneBillion + start.tv_nsec;
  const int64_t stop64 = stop.tv_sec * kOneBillion + stop.tv_nsec;

  const int64_t result64 = stop64 - start64;

  struct timespec ret;
  ret.tv_sec = result64 / kOneBillion;
  ret.tv_nsec = result64 % kOneBillion;

  return ret;
}
bool CPUClockTimeGreaterThanHalfSecond(const struct timespec& spec) {
  if (spec.tv_sec >= 1)
    return true;
  return (spec.tv_nsec > 500000000);
}
}

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

void UpdateAttempter::Update() {
  if (status_ == UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    LOG(INFO) << "Not updating b/c we already updated and we're waiting for "
              << "reboot";
    return;
  }
  if (status_ != UPDATE_STATUS_IDLE) {
    // Update in progress. Do nothing
    return;
  }
  if (!omaha_request_params_.Init()) {
    LOG(ERROR) << "Unable to initialize Omaha request device params.";
    return;
  }
  CHECK(!processor_.IsRunning());
  processor_.set_delegate(this);

  // Actions:
  shared_ptr<OmahaRequestAction> update_check_action(
      new OmahaRequestAction(omaha_request_params_,
                             NULL,
                             new LibcurlHttpFetcher));
  shared_ptr<OmahaResponseHandlerAction> response_handler_action(
      new OmahaResponseHandlerAction);
  shared_ptr<FilesystemCopierAction> filesystem_copier_action(
      new FilesystemCopierAction(false));
  shared_ptr<FilesystemCopierAction> kernel_filesystem_copier_action(
      new FilesystemCopierAction(true));
  shared_ptr<OmahaRequestAction> download_started_action(
      new OmahaRequestAction(omaha_request_params_,
                             new OmahaEvent(
                                 OmahaEvent::kTypeUpdateDownloadStarted),
                             new LibcurlHttpFetcher));
  shared_ptr<DownloadAction> download_action(
      new DownloadAction(new LibcurlHttpFetcher));
  shared_ptr<OmahaRequestAction> download_finished_action(
      new OmahaRequestAction(omaha_request_params_,
                             new OmahaEvent(
                                 OmahaEvent::kTypeUpdateDownloadFinished),
                             new LibcurlHttpFetcher));
  shared_ptr<PostinstallRunnerAction> postinstall_runner_action_precommit(
      new PostinstallRunnerAction(true));
  shared_ptr<SetBootableFlagAction> set_bootable_flag_action(
      new SetBootableFlagAction);
  shared_ptr<PostinstallRunnerAction> postinstall_runner_action_postcommit(
      new PostinstallRunnerAction(false));
  shared_ptr<OmahaRequestAction> update_complete_action(
      new OmahaRequestAction(omaha_request_params_,
                             new OmahaEvent(OmahaEvent::kTypeUpdateComplete),
                             new LibcurlHttpFetcher));

  download_action->set_delegate(this);
  response_handler_action_ = response_handler_action;

  actions_.push_back(shared_ptr<AbstractAction>(update_check_action));
  actions_.push_back(shared_ptr<AbstractAction>(response_handler_action));
  actions_.push_back(shared_ptr<AbstractAction>(filesystem_copier_action));
  actions_.push_back(shared_ptr<AbstractAction>(
      kernel_filesystem_copier_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_started_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_finished_action));
  actions_.push_back(shared_ptr<AbstractAction>(
      postinstall_runner_action_precommit));
  actions_.push_back(shared_ptr<AbstractAction>(set_bootable_flag_action));
  actions_.push_back(shared_ptr<AbstractAction>(
      postinstall_runner_action_postcommit));
  actions_.push_back(shared_ptr<AbstractAction>(update_complete_action));

  // Enqueue the actions
  for (vector<shared_ptr<AbstractAction> >::iterator it = actions_.begin();
       it != actions_.end(); ++it) {
    processor_.EnqueueAction(it->get());
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
              postinstall_runner_action_precommit.get());
  BondActions(postinstall_runner_action_precommit.get(),
              set_bootable_flag_action.get());
  BondActions(set_bootable_flag_action.get(),
              postinstall_runner_action_postcommit.get());

  SetStatusAndNotify(UPDATE_STATUS_CHECKING_FOR_UPDATE);
  processor_.StartProcessing();
}

void UpdateAttempter::CheckForUpdate() {
  if (status_ != UPDATE_STATUS_IDLE) {
    LOG(INFO) << "Check for update requested, but status is "
              << UpdateStatusToString(status_) << ", so not checking.";
    return;
  }
  Update();
}

// Delegate methods:
void UpdateAttempter::ProcessingDone(const ActionProcessor* processor,
                                     ActionExitCode code) {
  CHECK(response_handler_action_);
  LOG(INFO) << "Processing Done.";
  actions_.clear();

  if (status_ == UPDATE_STATUS_REPORTING_ERROR_EVENT) {
    LOG(INFO) << "Error event sent.";
    SetStatusAndNotify(UPDATE_STATUS_IDLE);
    return;
  }

  if (code == kActionCodeSuccess) {
    SetStatusAndNotify(UPDATE_STATUS_UPDATED_NEED_REBOOT);
    utils::WriteFile(kUpdateCompletedMarker, "", 0);

    // Report the time it took to update the system.
    int64_t update_time = time(NULL) - last_checked_time_;
    metrics_lib_->SendToUMA("Installer.UpdateTime",
                            static_cast<int>(update_time),  // sample
                            1,  // min = 1 second
                            20 * 60,  // max = 20 minutes
                            50);  // buckets
    return;
  }

  LOG(INFO) << "Update failed.";
  if (ScheduleErrorEventAction())
    return;
  SetStatusAndNotify(UPDATE_STATUS_IDLE);
}

void UpdateAttempter::ProcessingStopped(const ActionProcessor* processor) {
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
  // Reset download progress regardless of whether or not the download action
  // succeeded.
  const string type = action->Type();
  if (type == DownloadAction::StaticType())
    download_progress_ = 0.0;
  if (code != kActionCodeSuccess) {
    // On failure, schedule an error event to be sent to Omaha. For
    // now assume that Omaha response action failure means that
    // there's no update so don't send an event. Also, double check
    // that the failure has not occurred while sending an error event
    // -- in which case don't schedule another. This shouldn't really
    // happen but just in case...
    if (type != OmahaResponseHandlerAction::StaticType() &&
        status_ != UPDATE_STATUS_REPORTING_ERROR_EVENT) {
      CreatePendingErrorEvent(code);
    }
    return;
  }
  // Find out which action completed.
  if (type == OmahaResponseHandlerAction::StaticType()) {
    SetStatusAndNotify(UPDATE_STATUS_DOWNLOADING);
    OmahaResponseHandlerAction* omaha_response_handler_action =
        dynamic_cast<OmahaResponseHandlerAction*>(action);
    CHECK(omaha_response_handler_action);
    const InstallPlan& plan = omaha_response_handler_action->install_plan();
    last_checked_time_ = time(NULL);
    // TODO(adlr): put version in InstallPlan
    new_version_ = "0.0.0.0";
    new_size_ = plan.size;
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

void UpdateAttempter::BytesReceived(uint64_t bytes_received, uint64_t total) {
  if (status_ != UPDATE_STATUS_DOWNLOADING) {
    LOG(ERROR) << "BytesReceived called while not downloading.";
    return;
  }
  download_progress_ = static_cast<double>(bytes_received) /
      static_cast<double>(total);
  // We self throttle here
  timespec now;
  now.tv_sec = 0;
  now.tv_nsec = 0;
  if (GetCPUClockTime(&now) &&
      CPUClockTimeGreaterThanHalfSecond(
          CPUClockTimeElapsed(last_notify_time_, now))) {
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
  if (!dbus_service_)
    return;
  GetCPUClockTime(&last_notify_time_);
  update_engine_service_emit_status_update(
      dbus_service_,
      last_checked_time_,
      download_progress_,
      UpdateStatusToString(status_),
      new_version_.c_str(),
      new_size_);
}

void UpdateAttempter::CreatePendingErrorEvent(ActionExitCode code) {
  if (error_event_.get()) {
    // This shouldn't really happen.
    LOG(WARNING) << "There's already an existing pending error event.";
    return;
  }
  error_event_.reset(new OmahaEvent(OmahaEvent::kTypeUpdateComplete,
                                    OmahaEvent::kResultError,
                                    code));
}

bool UpdateAttempter::ScheduleErrorEventAction() {
  if (error_event_.get() == NULL)
    return false;

  shared_ptr<OmahaRequestAction> error_event_action(
      new OmahaRequestAction(omaha_request_params_,
                             error_event_.release(),  // Pass ownership.
                             new LibcurlHttpFetcher));
  actions_.push_back(shared_ptr<AbstractAction>(error_event_action));
  processor_.EnqueueAction(error_event_action.get());
  SetStatusAndNotify(UPDATE_STATUS_REPORTING_ERROR_EVENT);
  processor_.StartProcessing();
  return true;
}

}  // namespace chromeos_update_engine
