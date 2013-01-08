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

bool RealSystemState::Initialize(bool enable_gpio) {
  metrics_lib_.Init();

  if (!prefs_.Init(FilePath(kPrefsDirectory))) {
    LOG(ERROR) << "Failed to initialize preferences.";
    return false;
  }

  if (!payload_state_.Initialize(&prefs_))
    return false;

  // Initialize the GPIO handler as instructed.
  if (enable_gpio) {
    // A real GPIO handler. Defer GPIO discovery to ensure the udev has ample
    // time to export the devices. Also require that test mode is physically
    // queried at most once and the result cached, for a more consistent update
    // behavior.
    udev_iface_.reset(new StandardUdevInterface());
    file_descriptor_.reset(new EintrSafeFileDescriptor());
    gpio_handler_.reset(new StandardGpioHandler(udev_iface_.get(),
                                                file_descriptor_.get(),
                                                true, true));
  } else {
    // A no-op GPIO handler, always indicating a non-test mode.
    gpio_handler_.reset(new NoopGpioHandler(false));
  }

  // All is well. Initialization successful.
  return true;
}

bool RealSystemState::IsOOBEComplete() {
  return file_util::PathExists(FilePath(kOOBECompletedMarker));
}

}  // namespace chromeos_update_engine
