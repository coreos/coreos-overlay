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

bool RealSystemState::Initialize(bool enable_gpio, bool enable_connection_manager) {
  metrics_lib_.Init();

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

  // Create the update attempter.
  update_attempter_.reset(new UpdateAttempter(this, &dbus_));

  // All is well. Initialization successful.
  return true;
}

}  // namespace chromeos_update_engine
