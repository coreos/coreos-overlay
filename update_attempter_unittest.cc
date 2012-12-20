// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/file_util.h>
#include <gtest/gtest.h>
#include <policy/libpolicy.h>
#include <policy/mock_device_policy.h>

#include "update_engine/action_mock.h"
#include "update_engine/action_processor_mock.h"
#include "update_engine/filesystem_copier_action.h"
#include "update_engine/mock_dbus_interface.h"
#include "update_engine/mock_http_fetcher.h"
#include "update_engine/mock_payload_state.h"
#include "update_engine/mock_system_state.h"
#include "update_engine/postinstall_runner_action.h"
#include "update_engine/prefs.h"
#include "update_engine/prefs_mock.h"
#include "update_engine/test_utils.h"
#include "update_engine/update_attempter.h"
#include "update_engine/update_check_scheduler.h"

using std::string;
using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::Ne;
using testing::NiceMock;
using testing::Property;
using testing::Return;
using testing::SetArgumentPointee;

namespace chromeos_update_engine {

// Test a subclass rather than the main class directly so that we can mock out
// methods within the class. There're explicit unit tests for the mocked out
// methods.
class UpdateAttempterUnderTest : public UpdateAttempter {
 public:
  explicit UpdateAttempterUnderTest(MockSystemState* mock_system_state,
                                    MockDbusGlib* dbus)
      : UpdateAttempter(mock_system_state, dbus, NULL) {}
};

class UpdateAttempterTest : public ::testing::Test {
 protected:
  UpdateAttempterTest()
      : attempter_(&mock_system_state_, &dbus_),
        mock_connection_manager(&mock_system_state_),
        loop_(NULL) {
    mock_system_state_.set_connection_manager(&mock_connection_manager);
  }
  virtual void SetUp() {
    EXPECT_EQ(NULL, attempter_.dbus_service_);
    EXPECT_TRUE(attempter_.system_state_ != NULL);
    EXPECT_EQ(NULL, attempter_.update_check_scheduler_);
    EXPECT_EQ(0, attempter_.http_response_code_);
    EXPECT_EQ(utils::kCpuSharesNormal, attempter_.shares_);
    EXPECT_EQ(NULL, attempter_.manage_shares_source_);
    EXPECT_FALSE(attempter_.download_active_);
    EXPECT_EQ(UPDATE_STATUS_IDLE, attempter_.status_);
    EXPECT_EQ(0.0, attempter_.download_progress_);
    EXPECT_EQ(0, attempter_.last_checked_time_);
    EXPECT_EQ("0.0.0.0", attempter_.new_version_);
    EXPECT_EQ(0, attempter_.new_payload_size_);
    processor_ = new ActionProcessorMock();
    attempter_.processor_.reset(processor_);  // Transfers ownership.
    prefs_ = mock_system_state_.mock_prefs();
  }

  void QuitMainLoop();
  static gboolean StaticQuitMainLoop(gpointer data);

  void UpdateTestStart();
  void UpdateTestVerify();
  static gboolean StaticUpdateTestStart(gpointer data);
  static gboolean StaticUpdateTestVerify(gpointer data);

  void PingOmahaTestStart();
  static gboolean StaticPingOmahaTestStart(gpointer data);

  void ReadTrackFromPolicyTestStart();
  static gboolean StaticReadTrackFromPolicyTestStart(gpointer data);

  void ReadUpdateDisabledFromPolicyTestStart();
  static gboolean StaticReadUpdateDisabledFromPolicyTestStart(gpointer data);

  void ReadTargetVersionPrefixFromPolicyTestStart();
  static gboolean StaticReadTargetVersionPrefixFromPolicyTestStart(
      gpointer data);

  void ReadScatterFactorFromPolicyTestStart();
  static gboolean StaticReadScatterFactorFromPolicyTestStart(
      gpointer data);

  void DecrementUpdateCheckCountTestStart();
  static gboolean StaticDecrementUpdateCheckCountTestStart(
      gpointer data);

  void NoScatteringDoneDuringManualUpdateTestStart();
  static gboolean StaticNoScatteringDoneDuringManualUpdateTestStart(
      gpointer data);

  MockSystemState mock_system_state_;
  MockDbusGlib dbus_;
  UpdateAttempterUnderTest attempter_;
  ActionProcessorMock* processor_;
  NiceMock<PrefsMock>* prefs_; // shortcut to mock_system_state_->mock_prefs()
  MockConnectionManager mock_connection_manager;
  GMainLoop* loop_;
};

TEST_F(UpdateAttempterTest, ActionCompletedDownloadTest) {
  scoped_ptr<MockHttpFetcher> fetcher(new MockHttpFetcher("", 0, NULL));
  fetcher->FailTransfer(503);  // Sets the HTTP response code.
  DownloadAction action(prefs_, NULL, fetcher.release());
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _)).Times(0);
  attempter_.ActionCompleted(NULL, &action, kActionCodeSuccess);
  EXPECT_EQ(503, attempter_.http_response_code());
  EXPECT_EQ(UPDATE_STATUS_FINALIZING, attempter_.status());
  ASSERT_TRUE(attempter_.error_event_.get() == NULL);
}

TEST_F(UpdateAttempterTest, ActionCompletedErrorTest) {
  ActionMock action;
  EXPECT_CALL(action, Type()).WillRepeatedly(Return("ActionMock"));
  attempter_.status_ = UPDATE_STATUS_DOWNLOADING;
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(Return(false));
  attempter_.ActionCompleted(NULL, &action, kActionCodeError);
  ASSERT_TRUE(attempter_.error_event_.get() != NULL);
}

TEST_F(UpdateAttempterTest, ActionCompletedOmahaRequestTest) {
  scoped_ptr<MockHttpFetcher> fetcher(new MockHttpFetcher("", 0, NULL));
  fetcher->FailTransfer(500);  // Sets the HTTP response code.
  OmahaRequestParams params;
  OmahaRequestAction action(&mock_system_state_, &params, NULL,
                            fetcher.release(), false);
  ObjectCollectorAction<OmahaResponse> collector_action;
  BondActions(&action, &collector_action);
  OmahaResponse response;
  response.poll_interval = 234;
  action.SetOutputObject(response);
  UpdateCheckScheduler scheduler(&attempter_, NULL, &mock_system_state_);
  attempter_.set_update_check_scheduler(&scheduler);
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _)).Times(0);
  attempter_.ActionCompleted(NULL, &action, kActionCodeSuccess);
  EXPECT_EQ(500, attempter_.http_response_code());
  EXPECT_EQ(UPDATE_STATUS_IDLE, attempter_.status());
  EXPECT_EQ(234, scheduler.poll_interval());
  ASSERT_TRUE(attempter_.error_event_.get() == NULL);
}

TEST_F(UpdateAttempterTest, RunAsRootConstructWithUpdatedMarkerTest) {
  extern const char* kUpdateCompletedMarker;
  const FilePath kMarker(kUpdateCompletedMarker);
  EXPECT_EQ(0, file_util::WriteFile(kMarker, "", 0));
  UpdateAttempterUnderTest attempter(&mock_system_state_, &dbus_);
  EXPECT_EQ(UPDATE_STATUS_UPDATED_NEED_REBOOT, attempter.status());
  EXPECT_TRUE(file_util::Delete(kMarker, false));
}

TEST_F(UpdateAttempterTest, GetErrorCodeForActionTest) {
  extern ActionExitCode GetErrorCodeForAction(AbstractAction* action,
                                              ActionExitCode code);
  EXPECT_EQ(kActionCodeSuccess,
            GetErrorCodeForAction(NULL, kActionCodeSuccess));

  OmahaRequestParams params;
  MockSystemState mock_system_state;
  OmahaRequestAction omaha_request_action(&mock_system_state, &params, NULL,
                                          NULL, false);
  EXPECT_EQ(kActionCodeOmahaRequestError,
            GetErrorCodeForAction(&omaha_request_action, kActionCodeError));
  OmahaResponseHandlerAction omaha_response_handler_action(&mock_system_state_);
  EXPECT_EQ(kActionCodeOmahaResponseHandlerError,
            GetErrorCodeForAction(&omaha_response_handler_action,
                                  kActionCodeError));
  FilesystemCopierAction filesystem_copier_action(false, false);
  EXPECT_EQ(kActionCodeFilesystemCopierError,
            GetErrorCodeForAction(&filesystem_copier_action, kActionCodeError));
  PostinstallRunnerAction postinstall_runner_action;
  EXPECT_EQ(kActionCodePostinstallRunnerError,
            GetErrorCodeForAction(&postinstall_runner_action,
                                  kActionCodeError));
  ActionMock action_mock;
  EXPECT_CALL(action_mock, Type()).Times(1).WillOnce(Return("ActionMock"));
  EXPECT_EQ(kActionCodeError,
            GetErrorCodeForAction(&action_mock, kActionCodeError));
}

TEST_F(UpdateAttempterTest, DisableDeltaUpdateIfNeededTest) {
  attempter_.omaha_request_params_.delta_okay = true;
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(Return(false));
  attempter_.DisableDeltaUpdateIfNeeded();
  EXPECT_TRUE(attempter_.omaha_request_params_.delta_okay);
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(UpdateAttempter::kMaxDeltaUpdateFailures - 1),
          Return(true)));
  attempter_.DisableDeltaUpdateIfNeeded();
  EXPECT_TRUE(attempter_.omaha_request_params_.delta_okay);
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(UpdateAttempter::kMaxDeltaUpdateFailures),
          Return(true)));
  attempter_.DisableDeltaUpdateIfNeeded();
  EXPECT_FALSE(attempter_.omaha_request_params_.delta_okay);
  EXPECT_CALL(*prefs_, GetInt64(_, _)).Times(0);
  attempter_.DisableDeltaUpdateIfNeeded();
  EXPECT_FALSE(attempter_.omaha_request_params_.delta_okay);
}

TEST_F(UpdateAttempterTest, MarkDeltaUpdateFailureTest) {
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(Return(false))
      .WillOnce(DoAll(SetArgumentPointee<1>(-1), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<1>(1), Return(true)))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(UpdateAttempter::kMaxDeltaUpdateFailures),
          Return(true)));
  EXPECT_CALL(*prefs_, SetInt64(Ne(kPrefsDeltaUpdateFailures), _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*prefs_, SetInt64(kPrefsDeltaUpdateFailures, 1)).Times(2);
  EXPECT_CALL(*prefs_, SetInt64(kPrefsDeltaUpdateFailures, 2)).Times(1);
  EXPECT_CALL(*prefs_, SetInt64(kPrefsDeltaUpdateFailures,
                               UpdateAttempter::kMaxDeltaUpdateFailures + 1))
      .Times(1);
  for (int i = 0; i < 4; i ++)
    attempter_.MarkDeltaUpdateFailure();
}

TEST_F(UpdateAttempterTest, ScheduleErrorEventActionNoEventTest) {
  EXPECT_CALL(*processor_, EnqueueAction(_)).Times(0);
  EXPECT_CALL(*processor_, StartProcessing()).Times(0);
  EXPECT_CALL(*mock_system_state_.mock_payload_state(), UpdateFailed(_))
      .Times(0);
  OmahaResponse response;
  response.payload_urls.push_back("http://url");
  response.payload_urls.push_back("https://url");
  mock_system_state_.mock_payload_state()->SetResponse(response);
  attempter_.ScheduleErrorEventAction();
  EXPECT_EQ(0, mock_system_state_.mock_payload_state()->GetUrlIndex());
}

TEST_F(UpdateAttempterTest, ScheduleErrorEventActionTest) {
  EXPECT_CALL(*processor_,
              EnqueueAction(Property(&AbstractAction::Type,
                                     OmahaRequestAction::StaticType())))
      .Times(1);
  EXPECT_CALL(*processor_, StartProcessing()).Times(1);
  ActionExitCode err = kActionCodeError;
  EXPECT_CALL(*mock_system_state_.mock_payload_state(), UpdateFailed(err));
  attempter_.error_event_.reset(new OmahaEvent(OmahaEvent::kTypeUpdateComplete,
                                               OmahaEvent::kResultError,
                                               err));
  attempter_.ScheduleErrorEventAction();
  EXPECT_EQ(UPDATE_STATUS_REPORTING_ERROR_EVENT, attempter_.status());
}

TEST_F(UpdateAttempterTest, UpdateStatusToStringTest) {
  extern const char* UpdateStatusToString(UpdateStatus);
  EXPECT_STREQ("UPDATE_STATUS_IDLE", UpdateStatusToString(UPDATE_STATUS_IDLE));
  EXPECT_STREQ("UPDATE_STATUS_CHECKING_FOR_UPDATE",
               UpdateStatusToString(UPDATE_STATUS_CHECKING_FOR_UPDATE));
  EXPECT_STREQ("UPDATE_STATUS_UPDATE_AVAILABLE",
               UpdateStatusToString(UPDATE_STATUS_UPDATE_AVAILABLE));
  EXPECT_STREQ("UPDATE_STATUS_DOWNLOADING",
               UpdateStatusToString(UPDATE_STATUS_DOWNLOADING));
  EXPECT_STREQ("UPDATE_STATUS_VERIFYING",
               UpdateStatusToString(UPDATE_STATUS_VERIFYING));
  EXPECT_STREQ("UPDATE_STATUS_FINALIZING",
               UpdateStatusToString(UPDATE_STATUS_FINALIZING));
  EXPECT_STREQ("UPDATE_STATUS_UPDATED_NEED_REBOOT",
               UpdateStatusToString(UPDATE_STATUS_UPDATED_NEED_REBOOT));
  EXPECT_STREQ("UPDATE_STATUS_REPORTING_ERROR_EVENT",
               UpdateStatusToString(UPDATE_STATUS_REPORTING_ERROR_EVENT));
  EXPECT_STREQ("unknown status",
               UpdateStatusToString(static_cast<UpdateStatus>(-1)));
}

void UpdateAttempterTest::QuitMainLoop() {
  g_main_loop_quit(loop_);
}

gboolean UpdateAttempterTest::StaticQuitMainLoop(gpointer data) {
  reinterpret_cast<UpdateAttempterTest*>(data)->QuitMainLoop();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticUpdateTestStart(gpointer data) {
  reinterpret_cast<UpdateAttempterTest*>(data)->UpdateTestStart();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticUpdateTestVerify(gpointer data) {
  reinterpret_cast<UpdateAttempterTest*>(data)->UpdateTestVerify();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticPingOmahaTestStart(gpointer data) {
  reinterpret_cast<UpdateAttempterTest*>(data)->PingOmahaTestStart();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticReadTrackFromPolicyTestStart(
    gpointer data) {
  reinterpret_cast<UpdateAttempterTest*>(data)->ReadTrackFromPolicyTestStart();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticReadUpdateDisabledFromPolicyTestStart(
    gpointer data) {
  UpdateAttempterTest* ua_test = reinterpret_cast<UpdateAttempterTest*>(data);
  ua_test->ReadUpdateDisabledFromPolicyTestStart();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticReadTargetVersionPrefixFromPolicyTestStart(
    gpointer data) {
  UpdateAttempterTest* ua_test = reinterpret_cast<UpdateAttempterTest*>(data);
  ua_test->ReadTargetVersionPrefixFromPolicyTestStart();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticReadScatterFactorFromPolicyTestStart(
    gpointer data) {
  UpdateAttempterTest* ua_test = reinterpret_cast<UpdateAttempterTest*>(data);
  ua_test->ReadScatterFactorFromPolicyTestStart();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticDecrementUpdateCheckCountTestStart(
    gpointer data) {
  UpdateAttempterTest* ua_test = reinterpret_cast<UpdateAttempterTest*>(data);
  ua_test->DecrementUpdateCheckCountTestStart();
  return FALSE;
}

gboolean UpdateAttempterTest::StaticNoScatteringDoneDuringManualUpdateTestStart(
    gpointer data) {
  UpdateAttempterTest* ua_test = reinterpret_cast<UpdateAttempterTest*>(data);
  ua_test->NoScatteringDoneDuringManualUpdateTestStart();
  return FALSE;
}

namespace {
const string kActionTypes[] = {
  OmahaRequestAction::StaticType(),
  OmahaResponseHandlerAction::StaticType(),
  FilesystemCopierAction::StaticType(),
  FilesystemCopierAction::StaticType(),
  OmahaRequestAction::StaticType(),
  DownloadAction::StaticType(),
  OmahaRequestAction::StaticType(),
  FilesystemCopierAction::StaticType(),
  FilesystemCopierAction::StaticType(),
  PostinstallRunnerAction::StaticType(),
  OmahaRequestAction::StaticType()
};
}  // namespace {}

void UpdateAttempterTest::UpdateTestStart() {
  attempter_.set_http_response_code(200);
  InSequence s;
  for (size_t i = 0; i < arraysize(kActionTypes); ++i) {
    EXPECT_CALL(*processor_,
                EnqueueAction(Property(&AbstractAction::Type,
                                       kActionTypes[i]))).Times(1);
  }
  EXPECT_CALL(*processor_, StartProcessing()).Times(1);

  attempter_.Update("", "", false, false, false, false);
  g_idle_add(&StaticUpdateTestVerify, this);
}

void UpdateAttempterTest::UpdateTestVerify() {
  EXPECT_EQ(0, attempter_.http_response_code());
  EXPECT_EQ(&attempter_, processor_->delegate());
  EXPECT_EQ(arraysize(kActionTypes), attempter_.actions_.size());
  for (size_t i = 0; i < arraysize(kActionTypes); ++i) {
    EXPECT_EQ(kActionTypes[i], attempter_.actions_[i]->Type());
  }
  EXPECT_EQ(attempter_.response_handler_action_.get(),
            attempter_.actions_[1].get());
  DownloadAction* download_action =
      dynamic_cast<DownloadAction*>(attempter_.actions_[5].get());
  ASSERT_TRUE(download_action != NULL);
  EXPECT_EQ(&attempter_, download_action->delegate());
  EXPECT_EQ(UPDATE_STATUS_CHECKING_FOR_UPDATE, attempter_.status());
  g_main_loop_quit(loop_);
}

TEST_F(UpdateAttempterTest, UpdateTest) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticUpdateTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

void UpdateAttempterTest::PingOmahaTestStart() {
  EXPECT_CALL(*processor_,
              EnqueueAction(Property(&AbstractAction::Type,
                                     OmahaRequestAction::StaticType())))
      .Times(1);
  EXPECT_CALL(*processor_, StartProcessing()).Times(1);
  attempter_.PingOmaha();
  g_idle_add(&StaticQuitMainLoop, this);
}

TEST_F(UpdateAttempterTest, PingOmahaTest) {
  UpdateCheckScheduler scheduler(&attempter_, NULL, &mock_system_state_);
  scheduler.enabled_ = true;
  EXPECT_FALSE(scheduler.scheduled_);
  attempter_.set_update_check_scheduler(&scheduler);
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticPingOmahaTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
  EXPECT_EQ(UPDATE_STATUS_UPDATED_NEED_REBOOT, attempter_.status());
  EXPECT_EQ(true, scheduler.scheduled_);
}

TEST_F(UpdateAttempterTest, CreatePendingErrorEventTest) {
  ActionMock action;
  const ActionExitCode kCode = kActionCodeDownloadTransferError;
  attempter_.CreatePendingErrorEvent(&action, kCode);
  ASSERT_TRUE(attempter_.error_event_.get() != NULL);
  EXPECT_EQ(OmahaEvent::kTypeUpdateComplete, attempter_.error_event_->type);
  EXPECT_EQ(OmahaEvent::kResultError, attempter_.error_event_->result);
  EXPECT_EQ(kCode, attempter_.error_event_->error_code);
}

TEST_F(UpdateAttempterTest, CreatePendingErrorEventResumedTest) {
  OmahaResponseHandlerAction *response_action =
      new OmahaResponseHandlerAction(&mock_system_state_);
  response_action->install_plan_.is_resume = true;
  attempter_.response_handler_action_.reset(response_action);
  ActionMock action;
  const ActionExitCode kCode = kActionCodeInstallDeviceOpenError;
  attempter_.CreatePendingErrorEvent(&action, kCode);
  ASSERT_TRUE(attempter_.error_event_.get() != NULL);
  EXPECT_EQ(OmahaEvent::kTypeUpdateComplete, attempter_.error_event_->type);
  EXPECT_EQ(OmahaEvent::kResultError, attempter_.error_event_->result);
  EXPECT_EQ(kCode | kActionCodeResumedFlag,
            attempter_.error_event_->error_code);
}

TEST_F(UpdateAttempterTest, ReadTrackFromPolicy) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticReadTrackFromPolicyTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

void UpdateAttempterTest::ReadTrackFromPolicyTestStart() {
  // Tests that the update track (aka release channel) is properly fetched
  // from the device policy.

  policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
  attempter_.policy_provider_.reset(new policy::PolicyProvider(device_policy));

  EXPECT_CALL(*device_policy, LoadPolicy()).WillRepeatedly(Return(true));

  EXPECT_CALL(*device_policy, GetReleaseChannel(_))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<0>(std::string("canary-channel")),
          Return(true)));

  attempter_.Update("", "", false, false, false, false);
  EXPECT_EQ("canary-channel", attempter_.omaha_request_params_.app_track);

  g_idle_add(&StaticQuitMainLoop, this);
}

TEST_F(UpdateAttempterTest, ReadUpdateDisabledFromPolicy) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticReadUpdateDisabledFromPolicyTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

void UpdateAttempterTest::ReadUpdateDisabledFromPolicyTestStart() {
  // Tests that the update_disbled flag is properly fetched
  // from the device policy.

  policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
  attempter_.policy_provider_.reset(new policy::PolicyProvider(device_policy));

  EXPECT_CALL(*device_policy, LoadPolicy()).WillRepeatedly(Return(true));

  EXPECT_CALL(*device_policy, GetUpdateDisabled(_))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<0>(true),
          Return(true)));

  attempter_.Update("", "", false, false, false, false);
  EXPECT_TRUE(attempter_.omaha_request_params_.update_disabled);

  g_idle_add(&StaticQuitMainLoop, this);
}

TEST_F(UpdateAttempterTest, ReadTargetVersionPrefixFromPolicy) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticReadTargetVersionPrefixFromPolicyTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

void UpdateAttempterTest::ReadTargetVersionPrefixFromPolicyTestStart() {
  // Tests that the target_version_prefix value is properly fetched
  // from the device policy.

  const std::string target_version_prefix = "1412.";

  policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
  attempter_.policy_provider_.reset(new policy::PolicyProvider(device_policy));

  EXPECT_CALL(*device_policy, LoadPolicy()).WillRepeatedly(Return(true));

  EXPECT_CALL(*device_policy, GetTargetVersionPrefix(_))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<0>(target_version_prefix),
          Return(true)));

  attempter_.Update("", "", false, false, false, false);
  EXPECT_EQ(target_version_prefix.c_str(),
            attempter_.omaha_request_params_.target_version_prefix);

  g_idle_add(&StaticQuitMainLoop, this);
}


TEST_F(UpdateAttempterTest, ReadScatterFactorFromPolicy) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticReadScatterFactorFromPolicyTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

// Tests that the scatter_factor_in_seconds value is properly fetched
// from the device policy.
void UpdateAttempterTest::ReadScatterFactorFromPolicyTestStart() {
  int64 scatter_factor_in_seconds = 36000;

  policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
  attempter_.policy_provider_.reset(new policy::PolicyProvider(device_policy));

  EXPECT_CALL(*device_policy, LoadPolicy()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_system_state_, device_policy()).WillRepeatedly(
      Return(device_policy));

  EXPECT_CALL(*device_policy, GetScatterFactorInSeconds(_))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<0>(scatter_factor_in_seconds),
          Return(true)));

  attempter_.Update("", "", false, false, false, false);
  EXPECT_EQ(scatter_factor_in_seconds, attempter_.scatter_factor_.InSeconds());

  g_idle_add(&StaticQuitMainLoop, this);
}

TEST_F(UpdateAttempterTest, DecrementUpdateCheckCountTest) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticDecrementUpdateCheckCountTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

void UpdateAttempterTest::DecrementUpdateCheckCountTestStart() {
  // Tests that the scatter_factor_in_seconds value is properly fetched
  // from the device policy and is decremented if value > 0.
  int64 initial_value = 5;
  Prefs prefs;
  attempter_.prefs_ = &prefs;

  EXPECT_CALL(mock_system_state_,
              IsOOBEComplete()).WillRepeatedly(Return(true));

  string prefs_dir;
  EXPECT_TRUE(utils::MakeTempDirectory("/tmp/ue_ut_prefs.XXXXXX",
                                       &prefs_dir));
  ScopedDirRemover temp_dir_remover(prefs_dir);

  LOG_IF(ERROR, !prefs.Init(FilePath(prefs_dir)))
      << "Failed to initialize preferences.";
  EXPECT_TRUE(prefs.SetInt64(kPrefsUpdateCheckCount, initial_value));

  int64 scatter_factor_in_seconds = 10;

  policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
  attempter_.policy_provider_.reset(new policy::PolicyProvider(device_policy));

  EXPECT_CALL(*device_policy, LoadPolicy()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_system_state_, device_policy()).WillRepeatedly(
      Return(device_policy));

  EXPECT_CALL(*device_policy, GetScatterFactorInSeconds(_))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<0>(scatter_factor_in_seconds),
          Return(true)));

  attempter_.Update("", "", false, false, false, false);
  EXPECT_EQ(scatter_factor_in_seconds, attempter_.scatter_factor_.InSeconds());

  // Make sure the file still exists.
  EXPECT_TRUE(prefs.Exists(kPrefsUpdateCheckCount));

  int64 new_value;
  EXPECT_TRUE(prefs.GetInt64(kPrefsUpdateCheckCount, &new_value));
  EXPECT_EQ(initial_value - 1, new_value);

  EXPECT_TRUE(attempter_.omaha_request_params_.update_check_count_wait_enabled);

  // However, if the count is already 0, it's not decremented. Test that.
  initial_value = 0;
  EXPECT_TRUE(prefs.SetInt64(kPrefsUpdateCheckCount, initial_value));
  attempter_.Update("", "", false, false, false, false);
  EXPECT_TRUE(prefs.Exists(kPrefsUpdateCheckCount));
  EXPECT_TRUE(prefs.GetInt64(kPrefsUpdateCheckCount, &new_value));
  EXPECT_EQ(initial_value, new_value);

  g_idle_add(&StaticQuitMainLoop, this);
}

TEST_F(UpdateAttempterTest, NoScatteringDoneDuringManualUpdateTestStart) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  g_idle_add(&StaticNoScatteringDoneDuringManualUpdateTestStart, this);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

void UpdateAttempterTest::NoScatteringDoneDuringManualUpdateTestStart() {
  // Tests that no scattering logic is enabled if the update check
  // is manually done (as opposed to a scheduled update check)
  int64 initial_value = 8;
  Prefs prefs;
  attempter_.prefs_ = &prefs;

  EXPECT_CALL(mock_system_state_,
              IsOOBEComplete()).WillRepeatedly(Return(true));

  string prefs_dir;
  EXPECT_TRUE(utils::MakeTempDirectory("/tmp/ue_ut_prefs.XXXXXX",
                                       &prefs_dir));
  ScopedDirRemover temp_dir_remover(prefs_dir);

  LOG_IF(ERROR, !prefs.Init(FilePath(prefs_dir)))
      << "Failed to initialize preferences.";
  EXPECT_TRUE(prefs.SetInt64(kPrefsWallClockWaitPeriod, initial_value));
  EXPECT_TRUE(prefs.SetInt64(kPrefsUpdateCheckCount, initial_value));

  // make sure scatter_factor is non-zero as scattering is disabled
  // otherwise.
  int64 scatter_factor_in_seconds = 50;

  policy::MockDevicePolicy* device_policy = new policy::MockDevicePolicy();
  attempter_.policy_provider_.reset(new policy::PolicyProvider(device_policy));

  EXPECT_CALL(*device_policy, LoadPolicy()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_system_state_, device_policy()).WillRepeatedly(
      Return(device_policy));

  EXPECT_CALL(*device_policy, GetScatterFactorInSeconds(_))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<0>(scatter_factor_in_seconds),
          Return(true)));

  // pass true for is_user_initiated so we can test that the
  // scattering is disabled.
  attempter_.Update("", "", false, false, false, true);
  EXPECT_EQ(scatter_factor_in_seconds, attempter_.scatter_factor_.InSeconds());

  // Make sure scattering is disabled for manual (i.e. user initiated) update
  // checks and all artifacts are removed.
  EXPECT_FALSE(attempter_.omaha_request_params_.wall_clock_based_wait_enabled);
  EXPECT_FALSE(prefs.Exists(kPrefsWallClockWaitPeriod));
  EXPECT_TRUE(attempter_.omaha_request_params_.waiting_period.InSeconds() == 0);
  EXPECT_FALSE(attempter_.omaha_request_params_.
                 update_check_count_wait_enabled);
  EXPECT_FALSE(prefs.Exists(kPrefsUpdateCheckCount));

  g_idle_add(&StaticQuitMainLoop, this);
}

}  // namespace chromeos_update_engine
