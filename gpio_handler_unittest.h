// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_GPIO_HANDLER_UNITTEST_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_GPIO_HANDLER_UNITTEST_H__

// This file contains various definitions that are shared by different mock
// implementations that emulate GPIO behavior in the system.

// Some common strings used by the different cooperating mocks for this module.
// We use preprocessor constants to allow concatenation at compile-time.
#define MOCK_GPIO_CHIP_ID     "100"
#define MOCK_DUTFLAGA_GPIO_ID "101"
#define MOCK_DUTFLAGB_GPIO_ID "102"
#define MOCK_SYSFS_PREFIX     "/mock/sys/class/gpio"

namespace chromeos_update_engine {

// Mock GPIO identifiers, used by all mocks involved in unit testing the GPIO
// module. These represent the GPIOs which the unit tests can cover. They should
// generally match the GPIOs specified inside GpioHandler.
enum MockGpioId {
  kMockGpioIdDutflaga = 0,
  kMockGpioIdDutflagb,
  kMockGpioIdMax  // marker, do not remove!
};

// Mock GPIO directions, which are analogous to actual GPIO directions.
enum MockGpioDir {
  kMockGpioDirIn = 0,
  kMockGpioDirOut,
  kMockGpioDirMax  // marker, do not remove!
};

// Mock GPIO values, ditto.
enum MockGpioVal {
  kMockGpioValUp = 0,
  kMockGpioValDown,
  kMockGpioValMax  // marker, do not remove!
};

}  // chromeos_update_engine

#endif /* CHROMEOS_PLATFORM_UPDATE_ENGINE_GPIO_HANDLER_UNITTEST_H__ */
