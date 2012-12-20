// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__

#include <gmock/gmock.h>

#include <metrics/metrics_library_mock.h>
#include <policy/mock_device_policy.h>

#include "update_engine/mock_payload_state.h"
#include "update_engine/prefs_mock.h"
#include "update_engine/system_state.h"

namespace chromeos_update_engine {

// Mock the SystemStateInterface so that we could lie that
// OOBE is completed even when there's no such marker file, etc.
class MockSystemState : public SystemState {
 public:
  MockSystemState() : prefs_(&mock_prefs_) {
    mock_payload_state_.Initialize(&mock_prefs_);
  }
  virtual ~MockSystemState() {}

  MOCK_METHOD0(IsOOBEComplete, bool());
  MOCK_METHOD1(set_device_policy, void(const policy::DevicePolicy*));
  MOCK_CONST_METHOD0(device_policy, const policy::DevicePolicy*());

  virtual ConnectionManager* connection_manager() {
    return connection_manager_;
  }

  virtual MetricsLibraryInterface* metrics_lib() {
    return &mock_metrics_lib_;
  }

  virtual PrefsInterface* prefs() {
    return prefs_;
  }

  virtual PayloadStateInterface* payload_state() {
    return &mock_payload_state_;
  }

  // MockSystemState-specific public method.
  void set_connection_manager(ConnectionManager* connection_manager) {
    connection_manager_ = connection_manager;
  }

  MetricsLibraryMock* mock_metrics_lib() {
    return &mock_metrics_lib_;
  }

  void set_prefs(PrefsInterface* prefs) {
    prefs_ = prefs;
  }

  testing::NiceMock<PrefsMock> *mock_prefs() {
    return &mock_prefs_;
  }

  MockPayloadState* mock_payload_state() {
    return &mock_payload_state_;
  }

 private:
  // These are Mock objects or objects we own.
  MetricsLibraryMock mock_metrics_lib_;
  testing::NiceMock<PrefsMock> mock_prefs_;
  testing::NiceMock<MockPayloadState> mock_payload_state_;

  // These are pointers to objects which caller can override.
  PrefsInterface* prefs_;
  ConnectionManager* connection_manager_;
};

} // namespeace chromeos_update_engine

#endif // CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__
