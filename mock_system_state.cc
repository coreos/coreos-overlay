// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/mock_system_state.h"
#include "update_engine/update_attempter_mock.h"

namespace chromeos_update_engine {

// Mock the SystemStateInterface so that we could lie that
// OOBE is completed even when there's no such marker file, etc.
MockSystemState::MockSystemState()
  : default_request_params_(this),
    prefs_(&mock_prefs_) {
  request_params_ = &default_request_params_;
  mock_payload_state_.Initialize(&mock_prefs_);
  mock_update_attempter_ = new testing::NiceMock<UpdateAttempterMock>(
      this, &dbus_);
}

UpdateAttempter* MockSystemState::update_attempter() {
  return mock_update_attempter_;
}

} // namespeace chromeos_update_engine

