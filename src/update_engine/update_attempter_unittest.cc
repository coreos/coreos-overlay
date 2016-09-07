// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include <gtest/gtest.h>

#include "files/file_util.h"
#include "update_engine/action_mock.h"
#include "update_engine/action_processor_mock.h"
#include "update_engine/filesystem_copier_action.h"
#include "update_engine/kernel_copier_action.h"
#include "update_engine/kernel_verifier_action.h"
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
      : UpdateAttempter(mock_system_state, dbus) {}
};

class UpdateAttempterTest : public ::testing::Test {
 protected:
  UpdateAttempterTest()
      : attempter_(&mock_system_state_, &dbus_),
        loop_(NULL) {
    // We set the set_good_partition command to a non-existent path so it fails
    // to run it. This avoids the async call to the command and continues the
    // update process right away. Tests testing that behavior can override the
    // default set_good_partition command if needed.
    attempter_.set_good_partition_cmd_ = "/path/to/non-existent/command";
  }

  virtual void SetUp() {
    EXPECT_EQ(NULL, attempter_.dbus_service_);
    EXPECT_TRUE(attempter_.system_state_ != NULL);
    EXPECT_EQ(NULL, attempter_.update_check_scheduler_);
    EXPECT_EQ(0, attempter_.http_response_code_);
    EXPECT_FALSE(attempter_.download_active_);
    EXPECT_EQ(UPDATE_STATUS_IDLE, attempter_.status_);
    EXPECT_EQ(0.0, attempter_.download_progress_);
    EXPECT_EQ(0, attempter_.last_checked_time_);
    EXPECT_EQ("0.0.0.0", attempter_.new_version_);
    EXPECT_EQ(0, attempter_.new_payload_size_);
    processor_ = new NiceMock<ActionProcessorMock>();
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

  NiceMock<MockSystemState> mock_system_state_;
  NiceMock<MockDbusGlib> dbus_;
  UpdateAttempterUnderTest attempter_;
  NiceMock<ActionProcessorMock>* processor_;
  NiceMock<PrefsMock>* prefs_; // shortcut to mock_system_state_->mock_prefs()
  GMainLoop* loop_;
};

TEST_F(UpdateAttempterTest, ActionCompletedDownloadTest) {
  std::unique_ptr<MockHttpFetcher> fetcher(new MockHttpFetcher("", 0));
  fetcher->FailTransfer(503);  // Sets the HTTP response code.
  DownloadAction action(prefs_, fetcher.release());
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _)).Times(0);
  EXPECT_CALL(*(mock_system_state_.mock_payload_state()),
              DownloadComplete()).Times(1);
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
  EXPECT_CALL(*(mock_system_state_.mock_payload_state()),
              DownloadComplete()).Times(0);
  attempter_.ActionCompleted(NULL, &action, kActionCodeError);
  ASSERT_TRUE(attempter_.error_event_.get() != NULL);
}

TEST_F(UpdateAttempterTest, ActionCompletedOmahaRequestTest) {
  std::unique_ptr<MockHttpFetcher> fetcher(new MockHttpFetcher("", 0));
  fetcher->FailTransfer(500);  // Sets the HTTP response code.
  OmahaRequestAction action(&mock_system_state_, NULL,
                            fetcher.release(), false);
  ObjectCollectorAction<OmahaResponse> collector_action;
  BondActions(&action, &collector_action);
  OmahaResponse response;
  response.poll_interval = 234;
  action.SetOutputObject(response);
  UpdateCheckScheduler scheduler(&attempter_, &mock_system_state_);
  attempter_.set_update_check_scheduler(&scheduler);
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _)).Times(0);
  EXPECT_CALL(*(mock_system_state_.mock_payload_state()),
              DownloadComplete()).Times(0);
  attempter_.ActionCompleted(NULL, &action, kActionCodeSuccess);
  EXPECT_EQ(500, attempter_.http_response_code());
  EXPECT_EQ(UPDATE_STATUS_IDLE, attempter_.status());
  EXPECT_EQ(234, scheduler.poll_interval());
  ASSERT_TRUE(attempter_.error_event_.get() == NULL);
}

TEST_F(UpdateAttempterTest, BytesReceivedTest) {
  EXPECT_FALSE(attempter_.download_active_);
  attempter_.SetDownloadStatus(true);
  EXPECT_TRUE(attempter_.download_active_);
  EXPECT_CALL(*(mock_system_state_.mock_payload_state()),
              DownloadProgress(4)).Times(1);
  attempter_.BytesReceived(4, 4, 8);
  EXPECT_EQ(0.5, attempter_.download_progress_);
  EXPECT_EQ(UPDATE_STATUS_DOWNLOADING, attempter_.status());
}

TEST_F(UpdateAttempterTest, RunAsRootConstructWithUpdatedMarkerTest) {
  extern const char* kUpdateCompletedMarker;
  const files::FilePath kMarker(kUpdateCompletedMarker);
  EXPECT_EQ(0, files::WriteFile(kMarker, "", 0));
  UpdateAttempterUnderTest attempter(&mock_system_state_, &dbus_);
  EXPECT_EQ(UPDATE_STATUS_UPDATED_NEED_REBOOT, attempter.status());
  EXPECT_TRUE(files::DeleteFile(kMarker, false));
}

TEST_F(UpdateAttempterTest, GetErrorCodeForActionTest) {
  extern ActionExitCode GetErrorCodeForAction(AbstractAction* action,
                                              ActionExitCode code);
  EXPECT_EQ(kActionCodeSuccess,
            GetErrorCodeForAction(NULL, kActionCodeSuccess));

  MockSystemState mock_system_state;
  OmahaRequestAction omaha_request_action(&mock_system_state, NULL,
                                          NULL, false);
  EXPECT_EQ(kActionCodeOmahaRequestError,
            GetErrorCodeForAction(&omaha_request_action, kActionCodeError));
  OmahaResponseHandlerAction omaha_response_handler_action(&mock_system_state_);
  EXPECT_EQ(kActionCodeOmahaResponseHandlerError,
            GetErrorCodeForAction(&omaha_response_handler_action,
                                  kActionCodeError));
  FilesystemCopierAction filesystem_copier_action(false);
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
  attempter_.omaha_request_params_->set_delta_okay(true);
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(Return(false));
  attempter_.DisableDeltaUpdateIfNeeded();
  EXPECT_TRUE(attempter_.omaha_request_params_->delta_okay());
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(UpdateAttempter::kMaxDeltaUpdateFailures - 1),
          Return(true)));
  attempter_.DisableDeltaUpdateIfNeeded();
  EXPECT_TRUE(attempter_.omaha_request_params_->delta_okay());
  EXPECT_CALL(*prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(UpdateAttempter::kMaxDeltaUpdateFailures),
          Return(true)));
  attempter_.DisableDeltaUpdateIfNeeded();
  EXPECT_FALSE(attempter_.omaha_request_params_->delta_okay());
  EXPECT_CALL(*prefs_, GetInt64(_, _)).Times(0);
  attempter_.DisableDeltaUpdateIfNeeded();
  EXPECT_FALSE(attempter_.omaha_request_params_->delta_okay());
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

namespace {
const string kActionTypes[] = {
  OmahaRequestAction::StaticType(),
  OmahaResponseHandlerAction::StaticType(),
  FilesystemCopierAction::StaticType(),
  KernelCopierAction::StaticType(),
  OmahaRequestAction::StaticType(),
  DownloadAction::StaticType(),
  OmahaRequestAction::StaticType(),
  FilesystemCopierAction::StaticType(),
  KernelVerifierAction::StaticType(),
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

  attempter_.Update(false);
  g_idle_add(&StaticUpdateTestVerify, this);
}

void UpdateAttempterTest::UpdateTestVerify() {
  EXPECT_EQ(0, attempter_.http_response_code());
  EXPECT_EQ(&attempter_, processor_->delegate());
  EXPECT_EQ(arraysize(kActionTypes), attempter_.actions_.size());
  for (size_t i = 0; i < arraysize(kActionTypes); ++i) {
    EXPECT_EQ(kActionTypes[i], attempter_.actions_[i]->Type());
  }
  // Indexes in attempter_.actions_ align with the kActionTypes list.
  // actions_ itself is initialized by a series of push_back operations
  // in UpdateAttempter::BuildUpdateActions.
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
  UpdateCheckScheduler scheduler(&attempter_, &mock_system_state_);
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
  EXPECT_EQ(kCode | kActionCodeTestOmahaUrlFlag,
            attempter_.error_event_->error_code);
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
  EXPECT_EQ(kCode | kActionCodeResumedFlag | kActionCodeTestOmahaUrlFlag,
            attempter_.error_event_->error_code);
}

}  // namespace chromeos_update_engine
