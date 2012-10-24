// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__

#include <gmock/gmock.h>

#include <metrics/metrics_library_mock.h>
#include <policy/mock_device_policy.h>

#include "update_engine/system_state.h"

namespace chromeos_update_engine {

// Mock the SystemStateInterface so that we could lie that
// OOBE is completed even when there's no such marker file, etc.
class MockSystemState : public SystemState {
 public:
  MockSystemState() {
    // By default, provide a mock metrics library. If the caller wants,
    // they can override this by using set_metrics_lib() method.
    metrics_lib_ = &mock_metrics_lib_;
  }
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

  void set_metrics_lib(MetricsLibraryInterface* metrics_lib) {
    metrics_lib_ = metrics_lib;
  }
  virtual MetricsLibraryInterface* metrics_lib() {
    return metrics_lib_;
  }


 private:
  ConnectionManager* connection_manager_;
  MetricsLibraryMock mock_metrics_lib_;
  MetricsLibraryInterface* metrics_lib_;
};

} // namespeace chromeos_update_engine

#endif // CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__
