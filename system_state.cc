// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/file_util.h>

#include "update_engine/system_state.h"

namespace chromeos_update_engine {

static const char kOOBECompletedMarker[] = "/home/chronos/.oobe_completed";

RealSystemState::RealSystemState() : connection_manager_(this) {}

bool RealSystemState::IsOOBEComplete() {
  return file_util::PathExists(FilePath(kOOBECompletedMarker));
}

void RealSystemState::SetDevicePolicy(
    const policy::DevicePolicy* device_policy) {
  device_policy_ = device_policy;
}

const policy::DevicePolicy* RealSystemState::GetDevicePolicy() const {
  return device_policy_;
}

ConnectionManager* RealSystemState::GetConnectionManager() {
  return &connection_manager_;
}

}  // namespace chromeos_update_engine
