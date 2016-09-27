// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "update_engine/certificate_checker.h"
#include "update_engine/certificate_checker_mock.h"
#include "update_engine/mock_system_state.h"
#include "update_engine/update_attempter_mock.h"
#include "update_engine/update_check_scheduler.h"

using std::string;
using testing::_;
using testing::AllOf;
using testing::Assign;
using testing::Ge;
using testing::Le;
using testing::MockFunction;
using testing::Return;

namespace chromeos_update_engine {

namespace {
void FuzzRange(int interval, int fuzz, int* interval_min, int* interval_max) {
  *interval_min = interval - fuzz / 2;
  *interval_max = interval + fuzz - fuzz / 2;
}
}  // namespace {}

// Test a subclass rather than the main class directly so that we can mock out
// GLib and utils in tests. There're explicit unit test for the wrapper methods.
class UpdateCheckSchedulerUnderTest : public UpdateCheckScheduler {
 public:
  UpdateCheckSchedulerUnderTest(UpdateAttempter* update_attempter,
                                MockSystemState* mock_system_state)
      : UpdateCheckScheduler(update_attempter, mock_system_state),
        mock_system_state_(mock_system_state) {}

  MOCK_METHOD2(GTimeoutAddSeconds, guint(guint seconds, GSourceFunc function));
  MOCK_METHOD0(IsOfficialBuild, bool());

  MockSystemState* mock_system_state_;
};

class UpdateCheckSchedulerTest : public ::testing::Test {
 public:
  UpdateCheckSchedulerTest()
      : attempter_(&mock_system_state_, &dbus_),
        scheduler_(&attempter_, &mock_system_state_) {}

 protected:
  virtual void SetUp() {
    test_ = this;
    loop_ = NULL;
    EXPECT_EQ(&attempter_, scheduler_.update_attempter_);
    EXPECT_FALSE(scheduler_.enabled_);
    EXPECT_FALSE(scheduler_.scheduled_);
    EXPECT_EQ(0, scheduler_.last_interval_);
    EXPECT_EQ(0, scheduler_.poll_interval_);
    // Make sure singleton CertificateChecker has its members properly setup.
    CertificateChecker::set_system_state(&mock_system_state_);
    CertificateChecker::set_openssl_wrapper(&openssl_wrapper_);
  }

  virtual void TearDown() {
    test_ = NULL;
    loop_ = NULL;
  }

  static gboolean SourceCallback(gpointer data) {
    g_main_loop_quit(test_->loop_);
    // Forwards the call to the function mock so that expectations can be set.
    return test_->source_callback_.Call(data);
  }

  MockSystemState mock_system_state_;
  MockDbusGlib dbus_;
  OpenSSLWrapperMock openssl_wrapper_;
  UpdateAttempterMock attempter_;
  UpdateCheckSchedulerUnderTest scheduler_;
  MockFunction<gboolean(gpointer data)> source_callback_;
  GMainLoop* loop_;
  static UpdateCheckSchedulerTest* test_;
};

UpdateCheckSchedulerTest* UpdateCheckSchedulerTest::test_ = NULL;

TEST_F(UpdateCheckSchedulerTest, CanScheduleTest) {
  EXPECT_FALSE(scheduler_.CanSchedule());
  scheduler_.enabled_ = true;
  EXPECT_TRUE(scheduler_.CanSchedule());
  scheduler_.scheduled_ = true;
  EXPECT_FALSE(scheduler_.CanSchedule());
  scheduler_.enabled_ = false;
  EXPECT_FALSE(scheduler_.CanSchedule());
}

TEST_F(UpdateCheckSchedulerTest, ComputeNextIntervalAndFuzzBackoffTest) {
  int interval, fuzz;
  attempter_.set_http_response_code(500);
  int last_interval = UpdateCheckScheduler::kTimeoutPeriodicInterval + 50;
  scheduler_.last_interval_ = last_interval;
  scheduler_.ComputeNextIntervalAndFuzz(0, &interval, &fuzz);
  EXPECT_EQ(2 * last_interval, interval);
  EXPECT_EQ(2 * last_interval, fuzz);

  attempter_.set_http_response_code(503);
  scheduler_.last_interval_ =
    UpdateCheckScheduler::kTimeoutMaxBackoffInterval / 2 + 1;
  scheduler_.ComputeNextIntervalAndFuzz(0, &interval, &fuzz);
  EXPECT_EQ(UpdateCheckScheduler::kTimeoutMaxBackoffInterval, interval);
  EXPECT_EQ(UpdateCheckScheduler::kTimeoutMaxBackoffInterval, fuzz);
}

TEST_F(UpdateCheckSchedulerTest, ComputeNextIntervalAndFuzzPollTest) {
  int interval, fuzz;
  int poll_interval = UpdateCheckScheduler::kTimeoutPeriodicInterval + 50;
  scheduler_.set_poll_interval(poll_interval);
  scheduler_.ComputeNextIntervalAndFuzz(0, &interval, &fuzz);
  EXPECT_EQ(poll_interval, interval);
  EXPECT_EQ(poll_interval, fuzz);

  scheduler_.set_poll_interval(
      UpdateCheckScheduler::kTimeoutMaxBackoffInterval + 1);
  scheduler_.ComputeNextIntervalAndFuzz(0, &interval, &fuzz);
  EXPECT_EQ(UpdateCheckScheduler::kTimeoutMaxBackoffInterval, interval);
  EXPECT_EQ(UpdateCheckScheduler::kTimeoutMaxBackoffInterval, fuzz);

  scheduler_.set_poll_interval(
      UpdateCheckScheduler::kTimeoutPeriodicInterval - 1);
  scheduler_.ComputeNextIntervalAndFuzz(0, &interval, &fuzz);
  EXPECT_EQ(UpdateCheckScheduler::kTimeoutPeriodicInterval, interval);
  EXPECT_EQ(UpdateCheckScheduler::kTimeoutRegularFuzz, fuzz);
}

TEST_F(UpdateCheckSchedulerTest, ComputeNextIntervalAndFuzzPriorityTest) {
  int interval, fuzz;
  attempter_.set_http_response_code(500);
  scheduler_.last_interval_ =
    UpdateCheckScheduler::kTimeoutPeriodicInterval + 50;
  int poll_interval = UpdateCheckScheduler::kTimeoutPeriodicInterval + 100;
  scheduler_.set_poll_interval(poll_interval);
  scheduler_.ComputeNextIntervalAndFuzz(0, &interval, &fuzz);
  EXPECT_EQ(poll_interval, interval);
  EXPECT_EQ(poll_interval, fuzz);
}

TEST_F(UpdateCheckSchedulerTest, ComputeNextIntervalAndFuzzTest) {
  int interval, fuzz;
  scheduler_.ComputeNextIntervalAndFuzz(0, &interval, &fuzz);
  EXPECT_EQ(UpdateCheckScheduler::kTimeoutPeriodicInterval, interval);
  EXPECT_EQ(UpdateCheckScheduler::kTimeoutRegularFuzz, fuzz);
}

TEST_F(UpdateCheckSchedulerTest, GTimeoutAddSecondsTest) {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
  // Invokes the actual GLib wrapper method rather than the subclass mock.
  scheduler_.UpdateCheckScheduler::GTimeoutAddSeconds(0, SourceCallback);
  EXPECT_CALL(source_callback_, Call(&scheduler_)).Times(1);
  g_main_loop_run(loop_);
  g_main_loop_unref(loop_);
}

TEST_F(UpdateCheckSchedulerTest, IsOfficialBuildTest) {
  // Invokes the actual utils wrapper method rather than the subclass mock.
  EXPECT_TRUE(scheduler_.UpdateCheckScheduler::IsOfficialBuild());
}
TEST_F(UpdateCheckSchedulerTest, RunNonOfficialBuildTest) {
  scheduler_.enabled_ = true;
  EXPECT_CALL(scheduler_, IsOfficialBuild()).Times(1).WillOnce(Return(false));
  scheduler_.Run();
  EXPECT_FALSE(scheduler_.enabled_);
  EXPECT_EQ(NULL, attempter_.update_check_scheduler());
}

TEST_F(UpdateCheckSchedulerTest, RunTest) {
  int interval_min, interval_max;
  FuzzRange(UpdateCheckScheduler::kTimeoutInitialInterval,
            UpdateCheckScheduler::kTimeoutRegularFuzz,
            &interval_min,
            &interval_max);
  EXPECT_CALL(scheduler_, IsOfficialBuild()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(scheduler_,
              GTimeoutAddSeconds(AllOf(Ge(interval_min), Le(interval_max)),
                                 scheduler_.StaticCheck)).Times(1);
  scheduler_.Run();
  EXPECT_TRUE(scheduler_.enabled_);
  EXPECT_EQ(&scheduler_, attempter_.update_check_scheduler());
}

TEST_F(UpdateCheckSchedulerTest, ScheduleCheckDisabledTest) {
  EXPECT_CALL(scheduler_, GTimeoutAddSeconds(_, _)).Times(0);
  scheduler_.ScheduleCheck(250, 30);
  EXPECT_EQ(0, scheduler_.last_interval_);
  EXPECT_FALSE(scheduler_.scheduled_);
}

TEST_F(UpdateCheckSchedulerTest, ScheduleCheckEnabledTest) {
  int interval_min, interval_max;
  FuzzRange(100, 10, &interval_min,&interval_max);
  EXPECT_CALL(scheduler_,
              GTimeoutAddSeconds(AllOf(Ge(interval_min), Le(interval_max)),
                                 scheduler_.StaticCheck)).Times(1);
  scheduler_.enabled_ = true;
  scheduler_.ScheduleCheck(100, 10);
  EXPECT_EQ(100, scheduler_.last_interval_);
  EXPECT_TRUE(scheduler_.scheduled_);
}

TEST_F(UpdateCheckSchedulerTest, ScheduleCheckNegativeIntervalTest) {
  EXPECT_CALL(scheduler_, GTimeoutAddSeconds(0, scheduler_.StaticCheck))
      .Times(1);
  scheduler_.enabled_ = true;
  scheduler_.ScheduleCheck(-50, 20);
  EXPECT_TRUE(scheduler_.scheduled_);
}

TEST_F(UpdateCheckSchedulerTest, ScheduleNextCheckDisabledTest) {
  EXPECT_CALL(scheduler_, GTimeoutAddSeconds(_, _)).Times(0);
  scheduler_.ScheduleNextCheck(false);
}

TEST_F(UpdateCheckSchedulerTest, ScheduleNextCheckEnabledTest) {
  int interval_min, interval_max;
  FuzzRange(UpdateCheckScheduler::kTimeoutPeriodicInterval,
            UpdateCheckScheduler::kTimeoutRegularFuzz,
            &interval_min,
            &interval_max);
  EXPECT_CALL(scheduler_,
              GTimeoutAddSeconds(AllOf(Ge(interval_min), Le(interval_max)),
                                 scheduler_.StaticCheck)).Times(1);
  scheduler_.enabled_ = true;
  scheduler_.ScheduleNextCheck(false);
}

TEST_F(UpdateCheckSchedulerTest, SetUpdateStatusIdleDisabledTest) {
  EXPECT_CALL(scheduler_, GTimeoutAddSeconds(_, _)).Times(0);
  scheduler_.SetUpdateStatus(UPDATE_STATUS_IDLE, kUpdateNoticeUnspecified);
}

TEST_F(UpdateCheckSchedulerTest, SetUpdateStatusIdleEnabledTest) {
  int interval_min, interval_max;
  FuzzRange(UpdateCheckScheduler::kTimeoutPeriodicInterval,
            UpdateCheckScheduler::kTimeoutRegularFuzz,
            &interval_min,
            &interval_max);
  EXPECT_CALL(scheduler_,
              GTimeoutAddSeconds(AllOf(Ge(interval_min), Le(interval_max)),
                                 scheduler_.StaticCheck)).Times(1);
  scheduler_.enabled_ = true;
  scheduler_.SetUpdateStatus(UPDATE_STATUS_IDLE, kUpdateNoticeUnspecified);
}

TEST_F(UpdateCheckSchedulerTest, SetUpdateStatusNonIdleTest) {
  EXPECT_CALL(scheduler_, GTimeoutAddSeconds(_, _)).Times(0);
  scheduler_.SetUpdateStatus(UPDATE_STATUS_DOWNLOADING,
                             kUpdateNoticeUnspecified);
  scheduler_.enabled_ = true;
  scheduler_.SetUpdateStatus(UPDATE_STATUS_DOWNLOADING,
                             kUpdateNoticeUnspecified);
}

}  // namespace chromeos_update_engine
