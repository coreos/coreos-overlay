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

#include "base/time.h"
#include "update_engine/action_processor.h"
#include "update_engine/download_action.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/omaha_response_handler_action.h"

class MetricsLibraryInterface;
struct UpdateEngineService;

namespace chromeos_update_engine {

class UpdateCheckScheduler;

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
  UpdateAttempter(PrefsInterface* prefs, MetricsLibraryInterface* metrics_lib);
  virtual ~UpdateAttempter();

  // Checks for update and, if a newer version is available, attempts
  // to update the system. Non-empty |in_app_version| or
  // |in_update_url| prevents automatic detection of the parameter.
  virtual void Update(const std::string& app_version,
                      const std::string& omaha_url);

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

  UpdateStatus status() const { return status_; }

  int http_response_code() const { return http_response_code_; }
  void set_http_response_code(int code) { http_response_code_ = code; }

  void set_dbus_service(struct UpdateEngineService* dbus_service) {
    dbus_service_ = dbus_service;
  }

  UpdateCheckScheduler* update_check_scheduler() const {
    return update_check_scheduler_;
  }
  void set_update_check_scheduler(UpdateCheckScheduler* scheduler) {
    update_check_scheduler_ = scheduler;
  }

  // This is the D-Bus service entry point for going through an
  // update. If the current status is idle invokes Update.
  void CheckForUpdate(const std::string& app_version,
                      const std::string& omaha_url);

  // Initiates a reboot if the current state is
  // UPDATED_NEED_REBOOT. Returns true on sucess, false otherwise.
  bool RebootIfNeeded();

  // DownloadActionDelegate methods
  void SetDownloadStatus(bool active);
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

  // Sets the process priority to |priority| and updates |priority_|
  // if the new |priority| is different than the current |priority_|,
  // otherwise simply returns.
  void SetPriority(utils::ProcessPriority priority);

  // Set the process priority to low and sets up timeout events to
  // increase the priority gradually to high.
  void SetupPriorityManagement();

  // Resets the process priority to normal and destroys any scheduled
  // timeout sources.
  void CleanupPriorityManagement();

  // The process priority timeout source callback increases the
  // current priority by one step (low goes to normal, normal goes to
  // high). Returns true if the callback must be invoked again after a
  // timeout, or false if GLib can destroy this timeout source.
  static gboolean StaticManagePriorityCallback(gpointer data);
  bool ManagePriorityCallback();

  // Last status notification timestamp used for throttling. Use
  // monotonic TimeTicks to ensure that notifications are sent even if
  // the system clock is set back in the middle of an update.
  base::TimeTicks last_notify_time_;

  std::vector<std::tr1::shared_ptr<AbstractAction> > actions_;
  ActionProcessor processor_;

  // If non-null, this UpdateAttempter will send status updates over this
  // dbus service.
  UpdateEngineService* dbus_service_;

  // pointer to the OmahaResponseHandlerAction in the actions_ vector;
  std::tr1::shared_ptr<OmahaResponseHandlerAction> response_handler_action_;

  // Pointer to the preferences store interface.
  PrefsInterface* prefs_;

  // Pointer to the UMA metrics collection library.
  MetricsLibraryInterface* metrics_lib_;

  // The current UpdateCheckScheduler to notify of state transitions.
  UpdateCheckScheduler* update_check_scheduler_;

  // Pending error event, if any.
  scoped_ptr<OmahaEvent> error_event_;

  int http_response_code_;

  // Current process priority.
  utils::ProcessPriority priority_;

  // The process priority management timeout source.
  GSource* manage_priority_source_;

  // Set to true if an update download is active (and BytesReceived
  // will be called), set to false otherwise.
  bool download_active_;

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
