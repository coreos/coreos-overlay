// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_PAYLOAD_STATE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_PAYLOAD_STATE_H__

#include "gmock/gmock.h"
#include "update_engine/payload_state.h"

namespace chromeos_update_engine {

class MockPayloadState: public PayloadState {
 public:
  MOCK_METHOD1(UpdateFailed, void(ActionExitCode error));
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_PAYLOAD_STATE_H__
