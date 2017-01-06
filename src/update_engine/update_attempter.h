// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_ATTEMPTER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_ATTEMPTER_H__

#include <time.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <glib.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/action_processor.h"
#include "update_engine/dbus_interface.h"
#include "update_engine/download_action.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/omaha_response_handler_action.h"
#include "update_engine/system_state.h"

struct UpdateEngineService;

namespace chromeos_update_engine {

class UpdateCheckScheduler;

extern const char* kUpdateCompletedMarker;
extern const char* kVersionZero;

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

enum UpdateNotice {
  kUpdateNoticeUnspecified = 0,
  kUpdateNoticeTestAddrFailed,
};

const char* UpdateStatusToString(UpdateStatus status);

class UpdateAttempter : public ActionProcessorDelegate,
                        public DownloadActionDelegate {
 public:
  static const int kMaxDeltaUpdateFailures;

  UpdateAttempter(SystemState* system_state,
                  DbusGlibInterface* dbus_iface);
  virtual ~UpdateAttempter() = default;

  // Checks for update and, if a newer version is available, attempts to update
  // the system. Interactive should be true when called by the user (ie dbus).
  virtual void Update(bool interactive);

  // ActionProcessorDelegate methods:
  void ProcessingDone(const ActionProcessor* processor, ActionExitCode code);
  void ProcessingStopped(const ActionProcessor* processor);
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       ActionExitCode code);

  // Resets the current state to UPDATE_STATUS_IDLE.
  // Used by update_engine_client for restarting a new update without
  // having to reboot once the previous update has reached
  // UPDATE_STATUS_UPDATED_NEED_REBOOT state. This is used only
  // for testing purposes.
  bool ResetStatus();

  // Returns the current status in the out params. Returns true on success.
  bool GetStatus(int64_t* last_checked_time,
                 double* progress,
                 std::string* current_operation,
                 std::string* new_version,
                 int64_t* new_size);

  // Runs coreos-setgootroot, whose responsibility it is to mark the
  // currently booted partition has high priority/permanent/etc. The execution
  // is asynchronous. On completion, the action processor may be started
  // depending on the |start_action_processor_| field. Note that every update
  // attempt goes through this method.
  void UpdateBootFlags();

  // Subprocess::Exec callback.
  void CompleteUpdateBootFlags(int return_code);
  static void StaticCompleteUpdateBootFlags(int return_code,
                                            const std::string& output,
                                            void* p);

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

  // This is the internal entry point for going through an
  // update. If the current status is idle invokes Update.
  // This is called by the DBus implementation.
  void CheckForUpdate(bool interactive);

  // Initiates a reboot if the current state is
  // UPDATED_NEED_REBOOT. Returns true on sucess, false otherwise.
  bool RebootIfNeeded();

  // DownloadActionDelegate methods
  void SetDownloadStatus(bool active);
  void BytesReceived(uint64_t received, uint64_t progress, uint64_t total);

  // Broadcasts the current status over D-Bus.
  void BroadcastStatus();

  // Returns the special flags to be added to ActionExitCode values based on the
  // parameters used in the current update attempt.
  uint32_t GetErrorCodeFlags();

 private:
  friend class UpdateAttempterTest;
  FRIEND_TEST(UpdateAttempterTest, ActionCompletedDownloadTest);
  FRIEND_TEST(UpdateAttempterTest, ActionCompletedErrorTest);
  FRIEND_TEST(UpdateAttempterTest, ActionCompletedOmahaRequestTest);
  FRIEND_TEST(UpdateAttempterTest, BytesReceivedTest);
  FRIEND_TEST(UpdateAttempterTest, CreatePendingErrorEventTest);
  FRIEND_TEST(UpdateAttempterTest, CreatePendingErrorEventResumedTest);
  FRIEND_TEST(UpdateAttempterTest, DisableDeltaUpdateIfNeededTest);
  FRIEND_TEST(UpdateAttempterTest, MarkDeltaUpdateFailureTest);
  FRIEND_TEST(UpdateAttempterTest, ReadTrackFromPolicy);
  FRIEND_TEST(UpdateAttempterTest, PingOmahaTest);
  FRIEND_TEST(UpdateAttempterTest, ScheduleErrorEventActionNoEventTest);
  FRIEND_TEST(UpdateAttempterTest, ScheduleErrorEventActionTest);
  FRIEND_TEST(UpdateAttempterTest, UpdateTest);

  // Sets the status to the given status and notifies a status update over dbus.
  // Also accepts a supplement notice, which is delegated to the scheduler and
  // used for making better informed scheduling decisions (e.g. retry timeout).
  void SetStatusAndNotify(UpdateStatus status, UpdateNotice notice);

  // Sets up the download parameters after receiving the update check response.
  void SetupDownload();

  // Creates an error event object in |error_event_| to be included in an
  // OmahaRequestAction once the current action processor is done.
  void CreatePendingErrorEvent(AbstractAction* action, ActionExitCode code);

  // If there's a pending error event allocated in |error_event_|, schedules an
  // OmahaRequestAction with that event in the current processor, clears the
  // pending event, updates the status and returns true. Returns false
  // otherwise.
  bool ScheduleErrorEventAction();

  // Callback to start the action processor.
  static gboolean StaticStartProcessing(gpointer data);

  // Schedules an event loop callback to start the action processor. This is
  // scheduled asynchronously to unblock the event loop.
  void ScheduleProcessingStart();

  // Checks if a full update is needed and forces it by updating the Omaha
  // request params.
  void DisableDeltaUpdateIfNeeded();

  // If this was a delta update attempt that failed, count it so that a full
  // update can be tried when needed.
  void MarkDeltaUpdateFailure();

  // Sends a ping to Omaha.
  // This is used after an update has been applied and we're waiting for the
  // user to reboot.  This ping helps keep the number of actives count
  // accurate in case a user takes a long time to reboot the device after an
  // update has been applied.
  void PingOmaha();

  // Helper method of Update() to calculate the update-related parameters
  // from various sources and set the appropriate state. Please refer to
  // Update() method for the meaning of the parametes.
  bool CalculateUpdateParams(bool interactive);

  // Helper method of Update() to construct the sequence of actions to
  // be performed for an update check. Please refer to
  // Update() method for the meaning of the parametes.
  void BuildUpdateActions(bool interactive);

  // Last status notification timestamp used for throttling. Use monotonic
  // steady_clock to ensure that notifications are sent even if the system
  // clock is set back in the middle of an update.
  std::chrono::steady_clock::time_point last_notify_time_;

  std::vector<std::shared_ptr<AbstractAction> > actions_;
  std::unique_ptr<ActionProcessor> processor_;

  // External state of the system outside the update_engine process
  // carved out separately to mock out easily in unit tests.
  SystemState* system_state_;

  // If non-null, this UpdateAttempter will send status updates over this
  // dbus service.
  UpdateEngineService* dbus_service_;

  // Pointer to the OmahaResponseHandlerAction in the actions_ vector.
  std::shared_ptr<OmahaResponseHandlerAction> response_handler_action_;

  // Pointer to the DownloadAction in the actions_ vector.
  std::shared_ptr<DownloadAction> download_action_;

  // Pointer to the preferences store interface. This is just a cached
  // copy of system_state->prefs() because it's used in many methods and
  // is convenient this way.
  PrefsInterface* prefs_;

  // The current UpdateCheckScheduler to notify of state transitions.
  UpdateCheckScheduler* update_check_scheduler_;

  // Pending error event, if any.
  std::unique_ptr<OmahaEvent> error_event_;

  // If we should request a reboot even tho we failed the update
  bool fake_update_success_;

  // HTTP server response code from the last HTTP request action.
  int http_response_code_;

  // Set to true if an update download is active (and BytesReceived
  // will be called), set to false otherwise.
  bool download_active_;

  // For status:
  UpdateStatus status_;
  double download_progress_;
  int64_t last_checked_time_;
  std::string new_version_;
  int64_t new_payload_size_;

  // Common parameters for all Omaha requests.
  OmahaRequestParams* omaha_request_params_;

  // Originally, both of these flags are false. Once UpdateBootFlags is called,
  // |update_boot_flags_running_| is set to true. As soon as UpdateBootFlags
  // completes its asynchronous run, |update_boot_flags_running_| is reset to
  // false and |updated_boot_flags_| is set to true. From that point on there
  // will be no more changes to these flags.
  bool updated_boot_flags_;  // True if UpdateBootFlags has completed.
  bool update_boot_flags_running_;  // True if UpdateBootFlags is running.

  // The command to run to set the current system as good.
  std::string set_good_partition_cmd_ = "/usr/sbin/coreos-setgoodroot";

  // True if the action processor needs to be started by the boot flag updater.
  bool start_action_processor_;

  DISALLOW_COPY_AND_ASSIGN(UpdateAttempter);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_ATTEMPTER_H__
