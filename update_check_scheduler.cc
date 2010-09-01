// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_check_scheduler.h"

#include "update_engine/utils.h"

namespace chromeos_update_engine {

const int UpdateCheckScheduler::kTimeoutOnce = 7 * 60;  // at 7 minutes
const int UpdateCheckScheduler::kTimeoutPeriodic = 45 * 60;  // every 45 minutes
const int UpdateCheckScheduler::kTimeoutRegularFuzz = 10 * 60;  // +/- 5 minutes
const int UpdateCheckScheduler::kTimeoutMaxBackoff = 4 * 60 * 60;  // 4 hours

UpdateCheckScheduler::UpdateCheckScheduler(UpdateAttempter* update_attempter)
    : update_attempter_(update_attempter),
      enabled_(false),
      scheduled_(false),
      last_interval_(0),
      poll_interval_(0) {}

UpdateCheckScheduler::~UpdateCheckScheduler() {}

void UpdateCheckScheduler::Run() {
  enabled_ = false;
  update_attempter_->set_update_check_scheduler(NULL);

  if (!IsOfficialBuild()) {
    LOG(WARNING) << "Non-official build: periodic update checks disabled.";
    return;
  }
  if (IsBootDeviceRemovable()) {
    LOG(WARNING) << "Removable device boot: periodic update checks disabled.";
    return;
  }
  enabled_ = true;

  // Registers this scheduler with the update attempter so that scheduler can be
  // notified of update status changes.
  update_attempter_->set_update_check_scheduler(this);

  // Kicks off periodic update checks. The first check is scheduled
  // |kTimeoutOnce| seconds from now. Subsequent checks are scheduled by
  // ScheduleNextCheck, normally at |kTimeoutPeriodic|-second intervals.
  ScheduleCheck(kTimeoutOnce, kTimeoutRegularFuzz);
}

bool UpdateCheckScheduler::IsBootDeviceRemovable() {
  return utils::IsRemovableDevice(utils::RootDevice(utils::BootDevice()));
}

bool UpdateCheckScheduler::IsOfficialBuild() {
  return utils::IsOfficialBuild();
}

guint UpdateCheckScheduler::GTimeoutAddSeconds(guint interval,
                                               GSourceFunc function) {
  return g_timeout_add_seconds(interval, function, this);
}

void UpdateCheckScheduler::ScheduleCheck(int interval, int fuzz) {
  if (!CanSchedule()) {
    return;
  }
  last_interval_ = interval;
  interval = utils::FuzzInt(interval, fuzz);
  if (interval < 0) {
    interval = 0;
  }
  GTimeoutAddSeconds(interval, StaticCheck);
  scheduled_ = true;
  LOG(INFO) << "Next update check in " << interval << " seconds.";
}

gboolean UpdateCheckScheduler::StaticCheck(void* scheduler) {
  UpdateCheckScheduler* me = reinterpret_cast<UpdateCheckScheduler*>(scheduler);
  CHECK(me->scheduled_);
  me->scheduled_ = false;
  me->update_attempter_->Update("", "");
  // This check ensures that future update checks will be or are already
  // scheduled. The check should never fail. A check failure means that there's
  // a bug that will most likely prevent further automatic update checks. It
  // seems better to crash in such cases and restart the update_engine daemon
  // into, hopefully, a known good state.
  CHECK(me->update_attempter_->status() != UPDATE_STATUS_IDLE ||
        !me->CanSchedule());
  return FALSE;  // Don't run again.
}

void UpdateCheckScheduler::ComputeNextIntervalAndFuzz(int* next_interval,
                                                      int* next_fuzz) {
  int interval = 0;
  if (poll_interval_ > 0) {
    // Server-dictated poll interval.
    interval = poll_interval_;
    LOG(WARNING) << "Using server-dictated poll interval: " << interval;
  } else if (update_attempter_->http_response_code() == 500 ||
             update_attempter_->http_response_code() == 503) {
    // Implements exponential back off on 500 (Internal Server Error) and 503
    // (Service Unavailable) HTTP response codes.
    interval = 2 * last_interval_;
    LOG(WARNING) << "Exponential back off due to 500/503 HTTP response code.";
  }
  if (interval > kTimeoutMaxBackoff) {
    interval = kTimeoutMaxBackoff;
  }
  // Back off and server-dictated poll intervals are fuzzed by +/- |interval|/2.
  int fuzz = interval;

  // Ensures that under normal conditions the regular update check interval and
  // fuzz are used. Also covers the case where back off is required based on the
  // initial update check.
  if (interval < kTimeoutPeriodic) {
    interval = kTimeoutPeriodic;
    fuzz = kTimeoutRegularFuzz;
  }
  *next_interval = interval;
  *next_fuzz = fuzz;
}

void UpdateCheckScheduler::ScheduleNextCheck() {
  int interval, fuzz;
  ComputeNextIntervalAndFuzz(&interval, &fuzz);
  ScheduleCheck(interval, fuzz);
}

void UpdateCheckScheduler::SetUpdateStatus(UpdateStatus status) {
  if (status == UPDATE_STATUS_IDLE) {
    ScheduleNextCheck();
  }
}

}  // namespace chromeos_update_engine
