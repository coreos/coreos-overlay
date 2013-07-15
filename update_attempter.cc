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
#include <tr1/memory>
#include <vector>

#include <base/file_util.h>
#include <base/rand_util.h>
#include <glib.h>
#include <metrics/metrics_library.h>
#include <policy/libpolicy.h>
#include <policy/device_policy.h>

#include "update_engine/certificate_checker.h"
#include "update_engine/dbus_service.h"
#include "update_engine/download_action.h"
#include "update_engine/filesystem_copier_action.h"
#include "update_engine/gpio_handler.h"
#include "update_engine/libcurl_http_fetcher.h"
#include "update_engine/multi_range_http_fetcher.h"
#include "update_engine/omaha_request_action.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/omaha_response_handler_action.h"
#include "update_engine/payload_state_interface.h"
#include "update_engine/postinstall_runner_action.h"
#include "update_engine/prefs_interface.h"
#include "update_engine/subprocess.h"
#include "update_engine/system_state.h"
#include "update_engine/update_check_scheduler.h"

using base::TimeDelta;
using base::TimeTicks;
using google::protobuf::NewPermanentCallback;
using std::make_pair;
using std::tr1::shared_ptr;
using std::set;
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

UpdateAttempter::UpdateAttempter(SystemState* system_state,
                                 DbusGlibInterface* dbus_iface)
    : processor_(new ActionProcessor()),
      system_state_(system_state),
      dbus_service_(NULL),
      update_check_scheduler_(NULL),
      fake_update_success_(false),
      http_response_code_(0),
      shares_(utils::kCpuSharesNormal),
      manage_shares_source_(NULL),
      download_active_(false),
      status_(UPDATE_STATUS_IDLE),
      download_progress_(0.0),
      last_checked_time_(0),
      new_version_("0.0.0.0"),
      new_payload_size_(0),
      proxy_manual_checks_(0),
      obeying_proxies_(true),
      chrome_proxy_resolver_(dbus_iface),
      updated_boot_flags_(false),
      update_boot_flags_running_(false),
      start_action_processor_(false),
      policy_provider_(NULL),
      is_using_test_url_(false),
      is_test_mode_(false),
      is_test_update_attempted_(false) {
  prefs_ = system_state->prefs();
  omaha_request_params_ = system_state->request_params();
  if (utils::FileExists(kUpdateCompletedMarker))
    status_ = UPDATE_STATUS_UPDATED_NEED_REBOOT;
}

UpdateAttempter::~UpdateAttempter() {
  CleanupCpuSharesManagement();
}

void UpdateAttempter::Update(const string& app_version,
                             const string& omaha_url,
                             bool obey_proxies,
                             bool interactive,
                             bool is_test_mode) {
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

  if (!CalculateUpdateParams(app_version,
                             omaha_url,
                             obey_proxies,
                             interactive,
                             is_test_mode)) {
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

bool UpdateAttempter::CalculateUpdateParams(const string& app_version,
                                            const string& omaha_url,
                                            bool obey_proxies,
                                            bool interactive,
                                            bool is_test_mode) {
  http_response_code_ = 0;

  // Set the test mode flag for the current update attempt.
  is_test_mode_ = is_test_mode;

  // Lazy initialize the policy provider, or reload the latest policy data.
  if (!policy_provider_.get())
    policy_provider_.reset(new policy::PolicyProvider());
  policy_provider_->Reload();

  if (policy_provider_->device_policy_is_loaded()) {
    LOG(INFO) << "Device policies/settings present";

    const policy::DevicePolicy& device_policy =
                                policy_provider_->GetDevicePolicy();

    bool update_disabled = false;
    device_policy.GetUpdateDisabled(&update_disabled);
    omaha_request_params_->set_update_disabled(update_disabled);

    string target_version_prefix;
    device_policy.GetTargetVersionPrefix(&target_version_prefix);
    omaha_request_params_->set_target_version_prefix(target_version_prefix);

    system_state_->set_device_policy(&device_policy);

    set<string> allowed_types;
    string allowed_types_str;
    if (device_policy.GetAllowedConnectionTypesForUpdate(&allowed_types)) {
      set<string>::const_iterator iter;
      for (iter = allowed_types.begin(); iter != allowed_types.end(); ++iter)
        allowed_types_str += *iter + " ";
    }

    LOG(INFO) << "Networks over which updates are allowed per policy : "
              << (allowed_types_str.empty() ? "all" : allowed_types_str);
  } else {
    LOG(INFO) << "No device policies/settings present.";
    system_state_->set_device_policy(NULL);
  }

  CalculateScatteringParams(interactive);

  // Determine whether an alternative test address should be used.
  string omaha_url_to_use = omaha_url;
  if ((is_using_test_url_ = (omaha_url_to_use.empty() && is_test_mode_))) {
    omaha_url_to_use = kTestUpdateUrl;
    LOG(INFO) << "using alternative server address: " << omaha_url_to_use;
  }

  if (!omaha_request_params_->Init(app_version,
                                   omaha_url_to_use,
                                   interactive)) {
    LOG(ERROR) << "Unable to initialize Omaha request device params.";
    return false;
  }

  // Set the target channel iff ReleaseChannelDelegated policy is set to
  // false and a non-empty ReleaseChannel policy is present. If delegated
  // is true, we'll ignore ReleaseChannel policy value.
  if (system_state_->device_policy()) {
    bool delegated = false;
    system_state_->device_policy()->GetReleaseChannelDelegated(&delegated);
    if (delegated) {
      LOG(INFO) << "Channel settings are delegated to user by policy. "
                   "Ignoring ReleaseChannel policy value";
    }
    else {
      LOG(INFO) << "Channel settings are not delegated to the user by policy";
      string target_channel;
      system_state_->device_policy()->GetReleaseChannel(&target_channel);
      if (target_channel.empty()) {
        LOG(INFO) << "No ReleaseChannel specified in policy";
      } else {
        // Pass in false for powerwash_allowed until we add it to the policy
        // protobuf.
        LOG(INFO) << "Setting target channel from ReleaseChannel policy value";
        omaha_request_params_->SetTargetChannel(target_channel, false);
      }
    }
  }

  LOG(INFO) << "update_disabled = "
            << utils::ToString(omaha_request_params_->update_disabled())
            << ", target_version_prefix = "
            << omaha_request_params_->target_version_prefix()
            << ", scatter_factor_in_seconds = "
            << utils::FormatSecs(scatter_factor_.InSeconds());

  LOG(INFO) << "Wall Clock Based Wait Enabled = "
            << omaha_request_params_->wall_clock_based_wait_enabled()
            << ", Update Check Count Wait Enabled = "
            << omaha_request_params_->update_check_count_wait_enabled()
            << ", Waiting Period = " << utils::FormatSecs(
               omaha_request_params_->waiting_period().InSeconds());

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
  return true;
}

void UpdateAttempter::CalculateScatteringParams(bool interactive) {
  // Take a copy of the old scatter value before we update it, as
  // we need to update the waiting period if this value changes.
  TimeDelta old_scatter_factor = scatter_factor_;
  const policy::DevicePolicy* device_policy = system_state_->device_policy();
  if (device_policy) {
    int64 new_scatter_factor_in_secs = 0;
    device_policy->GetScatterFactorInSeconds(&new_scatter_factor_in_secs);
    if (new_scatter_factor_in_secs < 0) // sanitize input, just in case.
      new_scatter_factor_in_secs  = 0;
    scatter_factor_ = TimeDelta::FromSeconds(new_scatter_factor_in_secs);
  }

  bool is_scatter_enabled = false;
  if (scatter_factor_.InSeconds() == 0) {
    LOG(INFO) << "Scattering disabled since scatter factor is set to 0";
  } else if (interactive) {
    LOG(INFO) << "Scattering disabled as this is an interactive update check";
  } else if (!system_state_->IsOOBEComplete()) {
    LOG(INFO) << "Scattering disabled since OOBE is not complete yet";
  } else {
    is_scatter_enabled = true;
    LOG(INFO) << "Scattering is enabled";
  }

  if (is_scatter_enabled) {
    // This means the scattering policy is turned on.
    // Now check if we need to update the waiting period. The two cases
    // in which we'd need to update the waiting period are:
    // 1. First time in process or a scheduled check after a user-initiated one.
    //    (omaha_request_params_->waiting_period will be zero in this case).
    // 2. Admin has changed the scattering policy value.
    //    (new scattering value will be different from old one in this case).
    int64 wait_period_in_secs = 0;
    if (omaha_request_params_->waiting_period().InSeconds() == 0) {
      // First case. Check if we have a suitable value to set for
      // the waiting period.
      if (prefs_->GetInt64(kPrefsWallClockWaitPeriod, &wait_period_in_secs) &&
          wait_period_in_secs > 0 &&
          wait_period_in_secs <= scatter_factor_.InSeconds()) {
        // This means:
        // 1. There's a persisted value for the waiting period available.
        // 2. And that persisted value is still valid.
        // So, in this case, we should reuse the persisted value instead of
        // generating a new random value to improve the chances of a good
        // distribution for scattering.
        omaha_request_params_->set_waiting_period(
          TimeDelta::FromSeconds(wait_period_in_secs));
        LOG(INFO) << "Using persisted wall-clock waiting period: " <<
            utils::FormatSecs(
                omaha_request_params_->waiting_period().InSeconds());
      }
      else {
        // This means there's no persisted value for the waiting period
        // available or its value is invalid given the new scatter_factor value.
        // So, we should go ahead and regenerate a new value for the
        // waiting period.
        LOG(INFO) << "Persisted value not present or not valid ("
                  << utils::FormatSecs(wait_period_in_secs)
                  << ") for wall-clock waiting period.";
        GenerateNewWaitingPeriod();
      }
    } else if (scatter_factor_ != old_scatter_factor) {
      // This means there's already a waiting period value, but we detected
      // a change in the scattering policy value. So, we should regenerate the
      // waiting period to make sure it's within the bounds of the new scatter
      // factor value.
      GenerateNewWaitingPeriod();
    } else {
      // Neither the first time scattering is enabled nor the scattering value
      // changed. Nothing to do.
      LOG(INFO) << "Keeping current wall-clock waiting period: " <<
          utils::FormatSecs(
              omaha_request_params_->waiting_period().InSeconds());
    }

    // The invariant at this point is that omaha_request_params_->waiting_period
    // is non-zero no matter which path we took above.
    LOG_IF(ERROR, omaha_request_params_->waiting_period().InSeconds() == 0)
        << "Waiting Period should NOT be zero at this point!!!";

    // Since scattering is enabled, wall clock based wait will always be
    // enabled.
    omaha_request_params_->set_wall_clock_based_wait_enabled(true);

    // If we don't have any issues in accessing the file system to update
    // the update check count value, we'll turn that on as well.
    bool decrement_succeeded = DecrementUpdateCheckCount();
    omaha_request_params_->set_update_check_count_wait_enabled(
      decrement_succeeded);
  } else {
    // This means the scattering feature is turned off or disabled for
    // this particular update check. Make sure to disable
    // all the knobs and artifacts so that we don't invoke any scattering
    // related code.
    omaha_request_params_->set_wall_clock_based_wait_enabled(false);
    omaha_request_params_->set_update_check_count_wait_enabled(false);
    omaha_request_params_->set_waiting_period(TimeDelta::FromSeconds(0));
    prefs_->Delete(kPrefsWallClockWaitPeriod);
    prefs_->Delete(kPrefsUpdateCheckCount);
    // Don't delete the UpdateFirstSeenAt file as we don't want manual checks
    // that result in no-updates (e.g. due to server side throttling) to
    // cause update starvation by having the client generate a new
    // UpdateFirstSeenAt for each scheduled check that follows a manual check.
  }
}

void UpdateAttempter::GenerateNewWaitingPeriod() {
  omaha_request_params_->set_waiting_period(TimeDelta::FromSeconds(
      base::RandInt(1, scatter_factor_.InSeconds())));

  LOG(INFO) << "Generated new wall-clock waiting period: " << utils::FormatSecs(
                omaha_request_params_->waiting_period().InSeconds());

  // Do a best-effort to persist this in all cases. Even if the persistence
  // fails, we'll still be able to scatter based on our in-memory value.
  // The persistence only helps in ensuring a good overall distribution
  // across multiple devices if they tend to reboot too often.
  prefs_->SetInt64(kPrefsWallClockWaitPeriod,
                   omaha_request_params_->waiting_period().InSeconds());
}

void UpdateAttempter::BuildUpdateActions(bool interactive) {
  CHECK(!processor_->IsRunning());
  processor_->set_delegate(this);

  // Actions:
  LibcurlHttpFetcher* update_check_fetcher =
      new LibcurlHttpFetcher(GetProxyResolver(), system_state_, is_test_mode_);
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
      new FilesystemCopierAction(false, false));
  shared_ptr<FilesystemCopierAction> kernel_filesystem_copier_action(
      new FilesystemCopierAction(true, false));
  shared_ptr<OmahaRequestAction> download_started_action(
      new OmahaRequestAction(system_state_,
                             new OmahaEvent(
                                 OmahaEvent::kTypeUpdateDownloadStarted),
                             new LibcurlHttpFetcher(GetProxyResolver(),
                                                    system_state_,
                                                    is_test_mode_),
                             false));
  LibcurlHttpFetcher* download_fetcher =
      new LibcurlHttpFetcher(GetProxyResolver(), system_state_, is_test_mode_);
  download_fetcher->set_check_certificate(CertificateChecker::kDownload);
  shared_ptr<DownloadAction> download_action(
      new DownloadAction(prefs_,
                         system_state_,
                         new MultiRangeHttpFetcher(
                             download_fetcher)));  // passes ownership
  shared_ptr<OmahaRequestAction> download_finished_action(
      new OmahaRequestAction(system_state_,
                             new OmahaEvent(
                                 OmahaEvent::kTypeUpdateDownloadFinished),
                             new LibcurlHttpFetcher(GetProxyResolver(),
                                                    system_state_,
                                                    is_test_mode_),
                             false));
  shared_ptr<FilesystemCopierAction> filesystem_verifier_action(
      new FilesystemCopierAction(false, true));
  shared_ptr<FilesystemCopierAction> kernel_filesystem_verifier_action(
      new FilesystemCopierAction(true, true));
  shared_ptr<PostinstallRunnerAction> postinstall_runner_action(
      new PostinstallRunnerAction);
  shared_ptr<OmahaRequestAction> update_complete_action(
      new OmahaRequestAction(system_state_,
                             new OmahaEvent(OmahaEvent::kTypeUpdateComplete),
                             new LibcurlHttpFetcher(GetProxyResolver(),
                                                    system_state_,
                                                    is_test_mode_),
                             false));

  download_action->set_delegate(this);
  response_handler_action_ = response_handler_action;
  download_action_ = download_action;

  actions_.push_back(shared_ptr<AbstractAction>(update_check_action));
  actions_.push_back(shared_ptr<AbstractAction>(response_handler_action));
  actions_.push_back(shared_ptr<AbstractAction>(filesystem_copier_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_started_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_finished_action));
  actions_.push_back(shared_ptr<AbstractAction>(filesystem_verifier_action));
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
              download_action.get());
  BondActions(download_action.get(),
              filesystem_verifier_action.get());
  BondActions(filesystem_verifier_action.get(),
              postinstall_runner_action.get());
}

void UpdateAttempter::CheckForUpdate(const string& app_version,
                                     const string& omaha_url,
                                     bool interactive) {
  LOG(INFO) << "New update check requested";

  if (status_ != UPDATE_STATUS_IDLE) {
    LOG(INFO) << "Skipping update check because current status is "
              << UpdateStatusToString(status_);
    return;
  }

  // Read GPIO signals and determine whether this is an automated test scenario.
  // For safety, we only allow a test update to be performed once; subsequent
  // update requests will be carried out normally.
  bool is_test_mode = (!is_test_update_attempted_ &&
                       system_state_->gpio_handler()->IsTestModeSignaled());
  if (is_test_mode) {
    LOG(WARNING) << "this is a test mode update attempt!";
    is_test_update_attempted_ = true;
  }

  // Pass through the interactive flag, in case we want to simulate a scheduled
  // test.
  Update(app_version, omaha_url, true, interactive, is_test_mode);
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

  // Reset cpu shares back to normal.
  CleanupCpuSharesManagement();

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
    prefs_->SetString(kPrefsPreviousVersion,
                      omaha_request_params_->app_version());
    DeltaPerformer::ResetUpdateProgress(prefs_, false);

    // Since we're done with scattering fully at this point, this is the
    // safest point delete the state files, as we're sure that the status is
    // set to reboot (which means no more updates will be applied until reboot)
    // This deletion is required for correctness as we want the next update
    // check to re-create a new random number for the update check count.
    // Similarly, we also delete the wall-clock-wait period that was persisted
    // so that we start with a new random value for the next update check
    // after reboot so that the same device is not favored or punished in any
    // way.
    prefs_->Delete(kPrefsUpdateCheckCount);
    prefs_->Delete(kPrefsWallClockWaitPeriod);
    prefs_->Delete(kPrefsUpdateFirstSeenAt);
    LOG(INFO) << "Update successfully applied, waiting to reboot.";

    SetStatusAndNotify(UPDATE_STATUS_UPDATED_NEED_REBOOT,
                       kUpdateNoticeUnspecified);

    // Report the time it took to update the system.
    int64_t update_time = time(NULL) - last_checked_time_;
    if (!fake_update_success_)
      system_state_->metrics_lib()->SendToUMA(
          "Installer.UpdateTime",
           static_cast<int>(update_time),  // sample
           1,  // min = 1 second
           20 * 60,  // max = 20 minutes
           50);  // buckets

    // Also report the success code so that the percentiles can be
    // interpreted properly for the remaining error codes in UMA.
    utils::SendErrorCodeToUma(system_state_, code);
    return;
  }

  if (ScheduleErrorEventAction()) {
    return;
  }
  LOG(INFO) << "No update.";
  SetStatusAndNotify(UPDATE_STATUS_IDLE, kUpdateNoticeUnspecified);
}

void UpdateAttempter::ProcessingStopped(const ActionProcessor* processor) {
  // Reset cpu shares back to normal.
  CleanupCpuSharesManagement();
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
    SetupCpuSharesManagement();
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
      const FilePath kUpdateCompletedMarkerPath(kUpdateCompletedMarker);
      return file_util::Delete(kUpdateCompletedMarkerPath, false);
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
  vector<string> cmd(1, "/usr/sbin/coreos-setgoodroot");
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
      new_payload_size_);
}

uint32_t UpdateAttempter::GetErrorCodeFlags()  {
  uint32_t flags = 0;

  if (!utils::IsNormalBootMode())
    flags |= kActionCodeDevModeFlag;

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

  // Send it to Uma.
  LOG(INFO) << "Reporting the error event";
  utils::SendErrorCodeToUma(system_state_, error_event_->error_code);

  // Send it to Omaha.
  shared_ptr<OmahaRequestAction> error_event_action(
      new OmahaRequestAction(system_state_,
                             error_event_.release(),  // Pass ownership.
                             new LibcurlHttpFetcher(GetProxyResolver(),
                                                    system_state_,
                                                    is_test_mode_),
                             false));
  actions_.push_back(shared_ptr<AbstractAction>(error_event_action));
  processor_->EnqueueAction(error_event_action.get());
  SetStatusAndNotify(UPDATE_STATUS_REPORTING_ERROR_EVENT,
                     kUpdateNoticeUnspecified);
  processor_->StartProcessing();
  return true;
}

void UpdateAttempter::SetCpuShares(utils::CpuShares shares) {
  if (shares_ == shares) {
    return;
  }
  if (utils::SetCpuShares(shares)) {
    shares_ = shares;
    LOG(INFO) << "CPU shares = " << shares_;
  }
}

void UpdateAttempter::SetupCpuSharesManagement() {
  if (manage_shares_source_) {
    LOG(ERROR) << "Cpu shares timeout source hasn't been destroyed.";
    CleanupCpuSharesManagement();
  }
  const int kCpuSharesTimeout = 2 * 60 * 60;  // 2 hours
  manage_shares_source_ = g_timeout_source_new_seconds(kCpuSharesTimeout);
  g_source_set_callback(manage_shares_source_,
                        StaticManageCpuSharesCallback,
                        this,
                        NULL);
  g_source_attach(manage_shares_source_, NULL);
  SetCpuShares(utils::kCpuSharesLow);
}

void UpdateAttempter::CleanupCpuSharesManagement() {
  if (manage_shares_source_) {
    g_source_destroy(manage_shares_source_);
    manage_shares_source_ = NULL;
  }
  SetCpuShares(utils::kCpuSharesNormal);
}

gboolean UpdateAttempter::StaticManageCpuSharesCallback(gpointer data) {
  return reinterpret_cast<UpdateAttempter*>(data)->ManageCpuSharesCallback();
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

bool UpdateAttempter::ManageCpuSharesCallback() {
  SetCpuShares(utils::kCpuSharesNormal);
  manage_shares_source_ = NULL;
  return false;  // Destroy the timeout source.
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
                               new LibcurlHttpFetcher(GetProxyResolver(),
                                                      system_state_,
                                                      is_test_mode_),
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


bool UpdateAttempter::DecrementUpdateCheckCount() {
  int64 update_check_count_value;

  if (!prefs_->Exists(kPrefsUpdateCheckCount)) {
    // This file does not exist. This means we haven't started our update
    // check count down yet, so nothing more to do. This file will be created
    // later when we first satisfy the wall-clock-based-wait period.
    LOG(INFO) << "No existing update check count. That's normal.";
    return true;
  }

  if (prefs_->GetInt64(kPrefsUpdateCheckCount, &update_check_count_value)) {
    // Only if we're able to read a proper integer value, then go ahead
    // and decrement and write back the result in the same file, if needed.
    LOG(INFO) << "Update check count = " << update_check_count_value;

    if (update_check_count_value == 0) {
      // It could be 0, if, for some reason, the file didn't get deleted
      // when we set our status to waiting for reboot. so we just leave it
      // as is so that we can prevent another update_check wait for this client.
      LOG(INFO) << "Not decrementing update check count as it's already 0.";
      return true;
    }

    if (update_check_count_value > 0)
      update_check_count_value--;
    else
      update_check_count_value = 0;

    // Write out the new value of update_check_count_value.
    if (prefs_->SetInt64(kPrefsUpdateCheckCount, update_check_count_value)) {
      // We successfully wrote out te new value, so enable the
      // update check based wait.
      LOG(INFO) << "New update check count = " << update_check_count_value;
      return true;
    }
  }

  LOG(INFO) << "Deleting update check count state due to read/write errors.";

  // We cannot read/write to the file, so disable the update check based wait
  // so that we don't get stuck in this OS version by any chance (which could
  // happen if there's some bug that causes to read/write incorrectly).
  // Also attempt to delete the file to do our best effort to cleanup.
  prefs_->Delete(kPrefsUpdateCheckCount);
  return false;
}

}  // namespace chromeos_update_engine
