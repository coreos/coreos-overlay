// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_ATTEMPTER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_ATTEMPTER_H__

#include <time.h>
#include <tr1/memory>
#include <string>
#include <vector>
#include <glib.h>
#include "update_engine/action_processor.h"
#include "update_engine/download_action.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/omaha_response_handler_action.h"

class MetricsLibraryInterface;
struct UpdateEngineService;

namespace chromeos_update_engine {

extern const char* kUpdateCompletedMarker;

enum UpdateStatus {
  UPDATE_STATUS_IDLE = 0,
  UPDATE_STATUS_CHECKING_FOR_UPDATE,
  UPDATE_STATUS_UPDATE_AVAILABLE,
  UPDATE_STATUS_DOWNLOADING,
  UPDATE_STATUS_VERIFYING,
  UPDATE_STATUS_FINALIZING,
  UPDATE_STATUS_UPDATED_NEED_REBOOT,
  UPDATE_STATUS_REPORTING_ERROR_EVENT,
};

const char* UpdateStatusToString(UpdateStatus status);

class UpdateAttempter : public ActionProcessorDelegate,
                        public DownloadActionDelegate {
 public:
  UpdateAttempter(MetricsLibraryInterface* metrics_lib)
      : dbus_service_(NULL),
        metrics_lib_(metrics_lib),
        status_(UPDATE_STATUS_IDLE),
        download_progress_(0.0),
        last_checked_time_(0),
        new_version_("0.0.0.0"),
        new_size_(0) {
    last_notify_time_.tv_sec = 0;
    last_notify_time_.tv_nsec = 0;
    if (utils::FileExists(kUpdateCompletedMarker))
      status_ = UPDATE_STATUS_UPDATED_NEED_REBOOT;
  }
  void Update();

  // ActionProcessorDelegate methods:
  void ProcessingDone(const ActionProcessor* processor, ActionExitCode code);
  void ProcessingStopped(const ActionProcessor* processor);
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       ActionExitCode code);

  // Stop updating. An attempt will be made to record status to the disk
  // so that updates can be resumed later.
  void Terminate();

  // Try to resume from a previously Terminate()d update.
  void ResumeUpdating();

  // Returns the current status in the out params. Returns true on success.
  bool GetStatus(int64_t* last_checked_time,
                 double* progress,
                 std::string* current_operation,
                 std::string* new_version,
                 int64_t* new_size);

  void set_dbus_service(struct UpdateEngineService* dbus_service) {
    dbus_service_ = dbus_service;
  }

  void CheckForUpdate();

  // DownloadActionDelegate method
  void BytesReceived(uint64_t bytes_received, uint64_t total);

 private:
  // Sets the status to the given status and notifies a status update
  // over dbus.
  void SetStatusAndNotify(UpdateStatus status);

  // Creates an error event object in |error_event_| to be included in
  // an OmahaRequestAction once the current action processor is done.
  void CreatePendingErrorEvent(AbstractAction* action, ActionExitCode code);

  // If there's a pending error event allocated in |error_event_|,
  // schedules an OmahaRequestAction with that event in the current
  // processor, clears the pending event, updates the status and
  // returns true. Returns false otherwise.
  bool ScheduleErrorEventAction();

  struct timespec last_notify_time_;

  std::vector<std::tr1::shared_ptr<AbstractAction> > actions_;
  ActionProcessor processor_;

  // If non-null, this UpdateAttempter will send status updates over this
  // dbus service.
  UpdateEngineService* dbus_service_;

  // pointer to the OmahaResponseHandlerAction in the actions_ vector;
  std::tr1::shared_ptr<OmahaResponseHandlerAction> response_handler_action_;

  // Pointer to the UMA metrics collection library.
  MetricsLibraryInterface* metrics_lib_;

  // Pending error event, if any.
  scoped_ptr<OmahaEvent> error_event_;

  // For status:
  UpdateStatus status_;
  double download_progress_;
  int64_t last_checked_time_;
  std::string new_version_;
  int64_t new_size_;

  // Device paramaters common to all Omaha requests.
  OmahaRequestDeviceParams omaha_request_params_;

  DISALLOW_COPY_AND_ASSIGN(UpdateAttempter);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_ATTEMPTER_H__
