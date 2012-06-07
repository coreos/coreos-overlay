// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__

#include <gmock/gmock.h>

#include "update_engine/utils.h"

namespace chromeos_update_engine {

// Mock the SystemStateInterface so that we could lie that
// OOBE is completed even when there's no such marker file, etc.
class MockSystemState : public SystemState {
  public:
    MOCK_METHOD0(IsOOBEComplete, bool());
};

} // namespeace chromeos_update_engine

#endif // CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_SYSTEM_STATE_H__
