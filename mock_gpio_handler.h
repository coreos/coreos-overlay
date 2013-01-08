// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_GPIO_HANDLER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_GPIO_HANDLER_H__

#include "gmock/gmock.h"
#include "update_engine/gpio_handler.h"

namespace chromeos_update_engine {

class MockGpioHandler: public GpioHandler {
 public:
  MOCK_METHOD0(IsTestModeSignaled, bool());
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_GPIO_HANDLER_H__
