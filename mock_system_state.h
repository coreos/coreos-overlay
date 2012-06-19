// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__

#include <gmock/gmock.h>

#include <policy/mock_device_policy.h>
#include "update_engine/system_state.h"

namespace chromeos_update_engine {

// Mock the SystemStateInterface so that we could lie that
// OOBE is completed even when there's no such marker file, etc.
class MockSystemState : public SystemState {
 public:
  MockSystemState() {}
  virtual ~MockSystemState() {}

  MOCK_METHOD0(IsOOBEComplete, bool());
  MOCK_METHOD1(SetDevicePolicy, void(const policy::DevicePolicy*));
  MOCK_CONST_METHOD0(GetDevicePolicy, const policy::DevicePolicy*());

  void SetConnectionManager(ConnectionManager* connection_manager) {
    connection_manager_ = connection_manager;
  }

  virtual ConnectionManager* GetConnectionManager() {
    return connection_manager_;
  }

 private:
  ConnectionManager* connection_manager_;
};

} // namespeace chromeos_update_engine

#endif // CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__
