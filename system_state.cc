// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/file_util.h>

#include "update_engine/system_state.h"

namespace chromeos_update_engine {

static const char kOOBECompletedMarker[] = "/home/chronos/.oobe_completed";
static const char kPrefsDirectory[] = "/var/lib/update_engine/prefs";

RealSystemState::RealSystemState()
    : device_policy_(NULL),
      connection_manager_(this) {}

bool RealSystemState::Initialize() {
  metrics_lib_.Init();

  if (!prefs_.Init(FilePath(kPrefsDirectory))) {
    LOG(ERROR) << "Failed to initialize preferences.";
    return false;
  }

  if (!payload_state_.Initialize(&prefs_))
    return false;

  // All is well. Initialization successful.
  return true;
}

bool RealSystemState::IsOOBEComplete() {
  return file_util::PathExists(FilePath(kOOBECompletedMarker));
}

void RealSystemState::set_device_policy(
    const policy::DevicePolicy* device_policy) {
  device_policy_ = device_policy;
}

const policy::DevicePolicy* RealSystemState::device_policy() const {
  return device_policy_;
}

ConnectionManager* RealSystemState::connection_manager() {
  return &connection_manager_;
}

MetricsLibraryInterface* RealSystemState::metrics_lib() {
  return &metrics_lib_;
}

PrefsInterface* RealSystemState::prefs() {
  return &prefs_;
}

PayloadState* RealSystemState::payload_state() {
  return &payload_state_;
}

}  // namespace chromeos_update_engine
