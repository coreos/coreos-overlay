// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_check_scheduler.h"

#include "update_engine/certificate_checker.h"
#include "update_engine/http_common.h"
#include "update_engine/system_state.h"
#include "update_engine/utils.h"

namespace chromeos_update_engine {

// Default update check timeout interval/fuzz values, in seconds. Note that
// actual fuzz is within +/- half of the indicated value.
const int UpdateCheckScheduler::kTimeoutInitialInterval    =  7 * 60;
const int UpdateCheckScheduler::kTimeoutPeriodicInterval   = 45 * 60;
const int UpdateCheckScheduler::kTimeoutQuickInterval      =  1 * 60;
const int UpdateCheckScheduler::kTimeoutMaxBackoffInterval =  4 * 60 * 60;
const int UpdateCheckScheduler::kTimeoutRegularFuzz        = 10 * 60;

UpdateCheckScheduler::UpdateCheckScheduler(UpdateAttempter* update_attempter,
                                           SystemState* system_state)
    : update_attempter_(update_attempter),
      enabled_(false),
      scheduled_(false),
      last_interval_(0),
      poll_interval_(0),
      system_state_(system_state) {}

UpdateCheckScheduler::~UpdateCheckScheduler() {}

void UpdateCheckScheduler::Run() {
  enabled_ = false;
  update_attempter_->set_update_check_scheduler(NULL);

  if (!IsOfficialBuild()) {
    LOG(WARNING) << "Non-official build: periodic update checks disabled.";
    return;
  }
  enabled_ = true;

  // Registers this scheduler with the update attempter so that scheduler can be
  // notified of update status changes.
  update_attempter_->set_update_check_scheduler(this);

  // Kicks off periodic update checks. The first check is scheduled
  // |kTimeoutInitialInterval| seconds from now. Subsequent checks are scheduled
  // by ScheduleNextCheck, normally at |kTimeoutPeriodicInterval|-second
  // intervals.
  ScheduleCheck(kTimeoutInitialInterval, kTimeoutRegularFuzz);
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
  LOG(INFO) << "Next update check in " << utils::FormatSecs(interval);
}

gboolean UpdateCheckScheduler::StaticCheck(void* scheduler) {
  UpdateCheckScheduler* me = reinterpret_cast<UpdateCheckScheduler*>(scheduler);
  CHECK(me->scheduled_);
  me->scheduled_ = false;

  // Before updating, we flush any previously generated UMA reports.
  CertificateChecker::FlushReport();
  me->update_attempter_->Update(false);

  // This check ensures that future update checks will be or are already
  // scheduled. The check should never fail. A check failure means that there's
  // a bug that will most likely prevent further automatic update checks. It
  // seems better to crash in such cases and restart the update_engine daemon
  // into, hopefully, a known good state.
  CHECK(me->update_attempter_->status() != UPDATE_STATUS_IDLE ||
        !me->CanSchedule());
  return FALSE;  // Don't run again.
}

void UpdateCheckScheduler::ComputeNextIntervalAndFuzz(const int forced_interval,
                                                      int* next_interval,
                                                      int* next_fuzz) {
  CHECK(next_interval && next_fuzz);

  int interval = forced_interval;
  int fuzz = 0;  // Use default fuzz value (see below)

  if (interval == 0) {
    int http_response_code;
    if (poll_interval_ > 0) {
      // Server-dictated poll interval.
      interval = poll_interval_;
      LOG(WARNING) << "Using server-dictated poll interval: " << interval;
    } else if ((http_response_code = update_attempter_->http_response_code()) ==
               kHttpResponseInternalServerError ||
               http_response_code == kHttpResponseServiceUnavailable) {
      // Implements exponential backoff on 500 (Internal Server Error) and 503
      // (Service Unavailable) HTTP response codes.
      interval = 2 * last_interval_;
      LOG(WARNING) << "Exponential backoff due to HTTP response code ("
                   << http_response_code << ")";
    }

    // Backoff cannot exceed a predetermined maximum period.
    if (interval > kTimeoutMaxBackoffInterval)
      interval = kTimeoutMaxBackoffInterval;

    // Ensures that under normal conditions the regular update check interval
    // and fuzz are used. Also covers the case where backoff is required based
    // on the initial update check.
    if (interval < kTimeoutPeriodicInterval) {
      interval = kTimeoutPeriodicInterval;
      fuzz = kTimeoutRegularFuzz;
    }
  }

  // Set default fuzz to +/- |interval|/2.
  if (fuzz == 0)
    fuzz = interval;

  *next_interval = interval;
  *next_fuzz = fuzz;
}

void UpdateCheckScheduler::ScheduleNextCheck(bool is_force_quick) {
  int interval, fuzz;
  ComputeNextIntervalAndFuzz(is_force_quick ? kTimeoutQuickInterval : 0,
                             &interval, &fuzz);
  ScheduleCheck(interval, fuzz);
}

void UpdateCheckScheduler::SetUpdateStatus(UpdateStatus status,
                                           UpdateNotice notice) {
  // We want to schedule the update checks for when we're idle as well as
  // after we've successfully applied an update and waiting for the user
  // to reboot to ensure our active count is accurate.
  if (status == UPDATE_STATUS_IDLE ||
      status == UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    ScheduleNextCheck(notice == kUpdateNoticeTestAddrFailed);
  }
}

}  // namespace chromeos_update_engine
