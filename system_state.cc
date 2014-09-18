// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/file_util.h>

#include "update_engine/real_system_state.h"

namespace chromeos_update_engine {

static const char kPrefsDirectory[] = "/var/lib/update_engine/prefs";

RealSystemState::RealSystemState()
    : device_policy_(NULL),
      request_params_(this) {}

bool RealSystemState::Initialize(bool enable_connection_manager) {
  if (!prefs_.Init(FilePath(kPrefsDirectory))) {
    LOG(ERROR) << "Failed to initialize preferences.";
    return false;
  }

  if (!payload_state_.Initialize(&prefs_))
    return false;

  if (enable_connection_manager) {
    connection_manager_ = new ConnectionManager(this);
  } else {
    connection_manager_ = new NoopConnectionManager(this);
  }

  // Create the update attempter.
  update_attempter_.reset(new UpdateAttempter(this, &dbus_));

  // All is well. Initialization successful.
  return true;
}

}  // namespace chromeos_update_engine
