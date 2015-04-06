// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__

#include <gmock/gmock.h>

#include "update_engine/mock_dbus_interface.h"
#include "update_engine/mock_payload_state.h"
#include "update_engine/prefs_mock.h"
#include "update_engine/system_state.h"

namespace chromeos_update_engine {

class UpdateAttempterMock;

// Mock the SystemStateInterface so that we could lie that
// OOBE is completed even when there's no such marker file, etc.
class MockSystemState : public SystemState {
 public:
  MockSystemState();

  virtual ~MockSystemState() {}

  inline virtual PrefsInterface* prefs() {
    return prefs_;
  }

  inline virtual PayloadStateInterface* payload_state() {
    return &mock_payload_state_;
  }

  virtual UpdateAttempter* update_attempter();

  inline virtual OmahaRequestParams* request_params() {
    return request_params_;
  }

  inline void set_prefs(PrefsInterface* prefs) {
    prefs_ = prefs;
  }

  inline testing::NiceMock<PrefsMock> *mock_prefs() {
    return &mock_prefs_;
  }

  inline MockPayloadState* mock_payload_state() {
    return &mock_payload_state_;
  }

  inline void set_request_params(OmahaRequestParams* params) {
    request_params_ = params;
  }

 private:
  // These are Mock objects or objects we own.
  testing::NiceMock<PrefsMock> mock_prefs_;
  testing::NiceMock<MockPayloadState> mock_payload_state_;
  testing::NiceMock<UpdateAttempterMock>* mock_update_attempter_;
  MockDbusGlib dbus_;

  // These are the other object we own.
  OmahaRequestParams default_request_params_;

  // These are pointers to objects which caller can override.
  PrefsInterface* prefs_;
  OmahaRequestParams* request_params_;
};

} // namespeace chromeos_update_engine

#endif // CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__
