// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_ATTEMPTER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_ATTEMPTER_H__

#include <time.h>

#include <tr1/memory>
#include <string>
#include <vector>

#include <base/time.h>
#include <glib.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/action_processor.h"
#include "update_engine/chrome_browser_proxy_resolver.h"
#include "update_engine/download_action.h"
#include "update_engine/gpio_handler.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/omaha_response_handler_action.h"
#include "update_engine/proxy_resolver.h"
#include "update_engine/system_state.h"

class MetricsLibraryInterface;
struct UpdateEngineService;

namespace policy {
  class PolicyProvider;
}

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

enum UpdateNotice {
  kUpdateNoticeUnspecified = 0,
  kUpdateNoticeTestAddrFailed,
};

const char* UpdateStatusToString(UpdateStatus status);

class UpdateAttempter : public ActionProcessorDelegate,
                        public DownloadActionDelegate {
 public:
  static const int kMaxDeltaUpdateFailures;

  UpdateAttempter(PrefsInterface* prefs,
                  MetricsLibraryInterface* metrics_lib,
                  DbusGlibInterface* dbus_iface,
                  GpioHandler* gpio_handler,
                  SystemState* system_state);
  virtual ~UpdateAttempter();

  // Checks for update and, if a newer version is available, attempts to update
  // the system. Non-empty |in_app_version| or |in_update_url| prevents
  // automatic detection of the parameter.  If |obey_proxies| is true, the
  // update will likely respect Chrome's proxy setting. For security reasons, we
  // may still not honor them.  Interactive should be true if this was called
  // from the user (ie dbus).  |is_test| will lead to using an alternative test
  // server URL, if |omaha_url| is empty. |is_user_initiated| will be true
  // only if the update is being kicked off through dbus and will be false for
  // other types of kick off such as scheduled updates.
  virtual void Update(const std::string& app_version,
                      const std::string& omaha_url,
                      bool obey_proxies,
                      bool interactive,
                      bool is_test_mode,
                      bool is_user_initiated);

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

  // Runs chromeos-setgoodkernel, whose responsibility it is to mark the
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
  void CheckForUpdate(const std::string& app_version,
                      const std::string& omaha_url,
                      bool is_user_initiated);

  // Initiates a reboot if the current state is
  // UPDATED_NEED_REBOOT. Returns true on sucess, false otherwise.
  bool RebootIfNeeded();

  // DownloadActionDelegate methods
  void SetDownloadStatus(bool active);
  void BytesReceived(uint64_t bytes_received, uint64_t total);

  // Broadcasts the current status over D-Bus.
  void BroadcastStatus();

 private:
  // Update server URL for automated lab test.
  static const char* const kTestUpdateUrl;

  friend class UpdateAttempterTest;
  FRIEND_TEST(UpdateAttempterTest, ActionCompletedDownloadTest);
  FRIEND_TEST(UpdateAttempterTest, ActionCompletedErrorTest);
  FRIEND_TEST(UpdateAttempterTest, ActionCompletedOmahaRequestTest);
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

  // Sets the process priority to |priority| and updates |priority_| if the new
  // |priority| is different than the current |priority_|, otherwise simply
  // returns.
  void SetPriority(utils::ProcessPriority priority);

  // Sets the process priority to low and sets up timeout events to increase it.
  void SetupPriorityManagement();

  // Resets the process priority to normal and destroys any scheduled timeout
  // sources.
  void CleanupPriorityManagement();

  // The process priority timeout source callback sets the current priority to
  // normal. Returns false so that GLib destroys the timeout source.
  static gboolean StaticManagePriorityCallback(gpointer data);
  bool ManagePriorityCallback();

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

  ProxyResolver* GetProxyResolver() {
    return obeying_proxies_ ?
        reinterpret_cast<ProxyResolver*>(&chrome_proxy_resolver_) :
        reinterpret_cast<ProxyResolver*>(&direct_proxy_resolver_);
  }

  // Sends a ping to Omaha.
  // This is used after an update has been applied and we're waiting for the
  // user to reboot.  This ping helps keep the number of actives count
  // accurate in case a user takes a long time to reboot the device after an
  // update has been applied.
  void PingOmaha();

  // Helper method of Update() to calculate the update-related parameters
  // from various sources and set the appropriate state. Please refer to
  // Update() method for the meaning of the parametes.
  bool CalculateUpdateParams(const std::string& app_version,
                             const std::string& omaha_url,
                             bool obey_proxies,
                             bool interactive,
                             bool is_test,
                             bool is_user_initiated);

  // Calculates all the scattering related parameters (such as waiting period,
  // which type of scattering is enabled, etc.) and also updates/deletes
  // the corresponding prefs file used in scattering. Should be called
  // only after the device policy has been loaded and set in the system_state_.
  void CalculateScatteringParams(bool is_user_initiated);

  // Sets a random value for the omaha_request_params_.waiting_period
  // based on the current scatter_factor_ value.
  void GenerateNewWaitingPeriod();

  // Helper method of Update() to construct the sequence of actions to
  // be performed for an update check. Please refer to
  // Update() method for the meaning of the parametes.
  void BuildUpdateActions(bool interactive);

  // Decrements the count in the kUpdateCheckCountFilePath.
  // Returns True if successfully decremented, false otherwise.
  bool DecrementUpdateCheckCount();

  // Last status notification timestamp used for throttling. Use monotonic
  // TimeTicks to ensure that notifications are sent even if the system clock is
  // set back in the middle of an update.
  base::TimeTicks last_notify_time_;

  std::vector<std::tr1::shared_ptr<AbstractAction> > actions_;
  scoped_ptr<ActionProcessor> processor_;

  // If non-null, this UpdateAttempter will send status updates over this
  // dbus service.
  UpdateEngineService* dbus_service_;

  // Pointer to the OmahaResponseHandlerAction in the actions_ vector.
  std::tr1::shared_ptr<OmahaResponseHandlerAction> response_handler_action_;

  // Pointer to the DownloadAction in the actions_ vector.
  std::tr1::shared_ptr<DownloadAction> download_action_;

  // Pointer to the preferences store interface.
  PrefsInterface* prefs_;

  // Pointer to the UMA metrics collection library.
  MetricsLibraryInterface* metrics_lib_;

  // The current UpdateCheckScheduler to notify of state transitions.
  UpdateCheckScheduler* update_check_scheduler_;

  // Pending error event, if any.
  scoped_ptr<OmahaEvent> error_event_;

  // If we should request a reboot even tho we failed the update
  bool fake_update_success_;

  // HTTP server response code from the last HTTP request action.
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
  int64_t new_payload_size_;

  // Device paramaters common to all Omaha requests.
  OmahaRequestDeviceParams omaha_request_params_;

  // Number of consecutive manual update checks we've had where we obeyed
  // Chrome's proxy settings.
  int proxy_manual_checks_;

  // If true, this update cycle we are obeying proxies
  bool obeying_proxies_;

  // Our two proxy resolvers
  DirectProxyResolver direct_proxy_resolver_;
  ChromeBrowserProxyResolver chrome_proxy_resolver_;

  // Originally, both of these flags are false. Once UpdateBootFlags is called,
  // |update_boot_flags_running_| is set to true. As soon as UpdateBootFlags
  // completes its asynchronous run, |update_boot_flags_running_| is reset to
  // false and |updated_boot_flags_| is set to true. From that point on there
  // will be no more changes to these flags.
  bool updated_boot_flags_;  // True if UpdateBootFlags has completed.
  bool update_boot_flags_running_;  // True if UpdateBootFlags is running.

  // True if the action processor needs to be started by the boot flag updater.
  bool start_action_processor_;

  // Used for fetching information about the device policy.
  scoped_ptr<policy::PolicyProvider> policy_provider_;

  // A flag for indicating whether we are using a test server URL.
  bool is_using_test_url_;

  // If true, will induce a test mode update attempt.
  bool is_test_mode_;

  // A flag indicating whether a test update cycle was already attempted.
  bool is_test_update_attempted_;

  // GPIO handler object.
  GpioHandler* gpio_handler_;

  // The current scatter factor as found in the policy setting.
  base::TimeDelta scatter_factor_;

  // External state of the system outside the update_engine process
  // carved out separately to mock out easily in unit tests.
  SystemState* system_state_;

  DISALLOW_COPY_AND_ASSIGN(UpdateAttempter);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_ATTEMPTER_H__
