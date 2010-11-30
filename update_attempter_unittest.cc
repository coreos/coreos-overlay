// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/file_util.h>
#include <gtest/gtest.h>

#include "update_engine/action_mock.h"
#include "update_engine/action_processor_mock.h"
#include "update_engine/filesystem_copier_action.h"
#include "update_engine/mock_dbus_interface.h"
#include "update_engine/mock_http_fetcher.h"
#include "update_engine/postinstall_runner_action.h"
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
  UpdateAttempterUnderTest()
      : UpdateAttempter(NULL, NULL, &dbus_) {}
  MockDbusGlib dbus_;
};

class UpdateAttempterTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    EXPECT_EQ(NULL, attempter_.dbus_service_);
    EXPECT_EQ(NULL, attempter_.prefs_);
    EXPECT_EQ(NULL, attempter_.metrics_lib_);
    EXPECT_EQ(NULL, attempter_.update_check_scheduler_);
    EXPECT_EQ(0, attempter_.http_response_code_);
    EXPECT_EQ(utils::kProcessPriorityNormal, attempter_.priority_);
    EXPECT_EQ(NULL, attempter_.manage_priority_source_);
    EXPECT_FALSE(attempter_.download_active_);
    EXPECT_EQ(UPDATE_STATUS_IDLE, attempter_.status_);
    EXPECT_EQ(0.0, attempter_.download_progress_);
    EXPECT_EQ(0, attempter_.last_checked_time_);
    EXPECT_EQ("0.0.0.0", attempter_.new_version_);
    EXPECT_EQ(0, attempter_.new_size_);
    EXPECT_FALSE(attempter_.is_full_update_);
    processor_ = new ActionProcessorMock();
    attempter_.processor_.reset(processor_);  // Transfers ownership.
    attempter_.prefs_ = &prefs_;
  }

  UpdateAttempterUnderTest attempter_;
  ActionProcessorMock* processor_;
  NiceMock<PrefsMock> prefs_;
};

TEST_F(UpdateAttempterTest, ActionCompletedDownloadTest) {
  scoped_ptr<MockHttpFetcher> fetcher(new MockHttpFetcher("", 0, NULL));
  fetcher->FailTransfer(503);  // Sets the HTTP response code.
  DownloadAction action(&prefs_, fetcher.release());
  EXPECT_CALL(prefs_, GetInt64(kPrefsDeltaUpdateFailures, _)).Times(0);
  attempter_.ActionCompleted(NULL, &action, kActionCodeSuccess);
  EXPECT_EQ(503, attempter_.http_response_code());
  EXPECT_EQ(UPDATE_STATUS_FINALIZING, attempter_.status());
  ASSERT_TRUE(attempter_.error_event_.get() == NULL);
}

TEST_F(UpdateAttempterTest, ActionCompletedErrorTest) {
  ActionMock action;
  EXPECT_CALL(action, Type()).WillRepeatedly(Return("ActionMock"));
  attempter_.status_ = UPDATE_STATUS_DOWNLOADING;
  EXPECT_CALL(prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(Return(false));
  attempter_.ActionCompleted(NULL, &action, kActionCodeError);
  ASSERT_TRUE(attempter_.error_event_.get() != NULL);
}

TEST_F(UpdateAttempterTest, ActionCompletedOmahaRequestTest) {
  scoped_ptr<MockHttpFetcher> fetcher(new MockHttpFetcher("", 0, NULL));
  fetcher->FailTransfer(500);  // Sets the HTTP response code.
  OmahaRequestParams params;
  OmahaRequestAction action(&prefs_, params, NULL, fetcher.release());
  ObjectCollectorAction<OmahaResponse> collector_action;
  BondActions(&action, &collector_action);
  OmahaResponse response;
  response.poll_interval = 234;
  action.SetOutputObject(response);
  UpdateCheckScheduler scheduler(&attempter_);
  attempter_.set_update_check_scheduler(&scheduler);
  EXPECT_CALL(prefs_, GetInt64(kPrefsDeltaUpdateFailures, _)).Times(0);
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
  UpdateAttempterUnderTest attempter;
  EXPECT_EQ(UPDATE_STATUS_UPDATED_NEED_REBOOT, attempter.status());
  EXPECT_TRUE(file_util::Delete(kMarker, false));
}

TEST_F(UpdateAttempterTest, GetErrorCodeForActionTest) {
  extern ActionExitCode GetErrorCodeForAction(AbstractAction* action,
                                              ActionExitCode code);
  EXPECT_EQ(kActionCodeSuccess,
            GetErrorCodeForAction(NULL, kActionCodeSuccess));

  OmahaRequestParams params;
  OmahaRequestAction omaha_request_action(NULL, params, NULL, NULL);
  EXPECT_EQ(kActionCodeOmahaRequestError,
            GetErrorCodeForAction(&omaha_request_action, kActionCodeError));
  OmahaResponseHandlerAction omaha_response_handler_action(&prefs_);
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
  attempter_.omaha_request_params_.delta_okay = true;
  EXPECT_CALL(prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(Return(false));
  attempter_.DisableDeltaUpdateIfNeeded();
  EXPECT_TRUE(attempter_.omaha_request_params_.delta_okay);
  EXPECT_CALL(prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(UpdateAttempter::kMaxDeltaUpdateFailures - 1),
          Return(true)));
  attempter_.DisableDeltaUpdateIfNeeded();
  EXPECT_TRUE(attempter_.omaha_request_params_.delta_okay);
  EXPECT_CALL(prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(UpdateAttempter::kMaxDeltaUpdateFailures),
          Return(true)));
  attempter_.DisableDeltaUpdateIfNeeded();
  EXPECT_FALSE(attempter_.omaha_request_params_.delta_okay);
  EXPECT_CALL(prefs_, GetInt64(_, _)).Times(0);
  attempter_.DisableDeltaUpdateIfNeeded();
  EXPECT_FALSE(attempter_.omaha_request_params_.delta_okay);
}

TEST_F(UpdateAttempterTest, MarkDeltaUpdateFailureTest) {
  attempter_.is_full_update_ = false;
  EXPECT_CALL(prefs_, GetInt64(kPrefsDeltaUpdateFailures, _))
      .WillOnce(Return(false))
      .WillOnce(DoAll(SetArgumentPointee<1>(-1), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<1>(1), Return(true)))
      .WillOnce(DoAll(
          SetArgumentPointee<1>(UpdateAttempter::kMaxDeltaUpdateFailures),
          Return(true)));
  EXPECT_CALL(prefs_, SetInt64(Ne(kPrefsDeltaUpdateFailures), _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(prefs_, SetInt64(kPrefsDeltaUpdateFailures, 1)).Times(2);
  EXPECT_CALL(prefs_, SetInt64(kPrefsDeltaUpdateFailures, 2)).Times(1);
  EXPECT_CALL(prefs_, SetInt64(kPrefsDeltaUpdateFailures,
                               UpdateAttempter::kMaxDeltaUpdateFailures + 1))
      .Times(1);
  for (int i = 0; i < 4; i ++)
    attempter_.MarkDeltaUpdateFailure();
}

TEST_F(UpdateAttempterTest, ScheduleErrorEventActionNoEventTest) {
  EXPECT_CALL(*processor_, EnqueueAction(_)).Times(0);
  EXPECT_CALL(*processor_, StartProcessing()).Times(0);
  attempter_.ScheduleErrorEventAction();
}

TEST_F(UpdateAttempterTest, ScheduleErrorEventActionTest) {
  EXPECT_CALL(*processor_,
              EnqueueAction(Property(&AbstractAction::Type,
                                     OmahaRequestAction::StaticType())))
      .Times(1);
  EXPECT_CALL(*processor_, StartProcessing()).Times(1);
  attempter_.error_event_.reset(new OmahaEvent(OmahaEvent::kTypeUpdateComplete,
                                               OmahaEvent::kResultError,
                                               kActionCodeError));
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

TEST_F(UpdateAttempterTest, UpdateTest) {
  attempter_.set_http_response_code(200);
  InSequence s;
  const string kActionTypes[] = {
    OmahaRequestAction::StaticType(),
    OmahaResponseHandlerAction::StaticType(),
    FilesystemCopierAction::StaticType(),
    FilesystemCopierAction::StaticType(),
    OmahaRequestAction::StaticType(),
    DownloadAction::StaticType(),
    OmahaRequestAction::StaticType(),
    PostinstallRunnerAction::StaticType(),
    OmahaRequestAction::StaticType()
  };
  for (size_t i = 0; i < arraysize(kActionTypes); ++i) {
    EXPECT_CALL(*processor_,
                EnqueueAction(Property(&AbstractAction::Type,
                                       kActionTypes[i]))).Times(1);
  }
  EXPECT_CALL(*processor_, StartProcessing()).Times(1);

  attempter_.Update("", "", false);

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
}

}  // namespace chromeos_update_engine
