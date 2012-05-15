// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "update_engine/gpio_handler.h"
#include "update_engine/gpio_mock_udev_interface.h"

// Some common strings used by the different cooperating mocks for this module.
// We use preprocessor constants to allow concatenation at compile-time.
#define MOCK_GPIO_CHIP_ID     "100"
#define MOCK_DUTFLAGA_GPIO_ID "101"
#define MOCK_DUTFLAGB_GPIO_ID "102"
#define MOCK_SYSFS_PREFIX     "/mock/sys/class/gpio"

namespace chromeos_update_engine {

class StandardGpioHandlerTest : public ::testing::Test {};

TEST(StandardGpioHandlerTest, NormalInitTest) {
  // Ensure that initialization of the GPIO module works as expected, and that
  // all udev resources are deallocated afterwards.  The mock file descriptor is
  // not to be used.
  StandardGpioMockUdevInterface mock_udev;
  StandardGpioHandler gpio_hander(&mock_udev, false);
  mock_udev.ExpectAllResourcesDeallocated();
  mock_udev.ExpectDiscoverySuccess();
}

TEST(StandardGpioHandlerTest, MultiGpioChipInitTest) {
  // Attempt GPIO discovery with a udev mock that returns two GPIO chip devices.
  // It should fail, of course.  The mock file descriptor is not to be used.
  MultiChipGpioMockUdevInterface mock_udev;
  StandardGpioHandler gpio_handler(&mock_udev, false);
  mock_udev.ExpectAllResourcesDeallocated();
  mock_udev.ExpectDiscoveryFail();
}

TEST(StandardGpioHandlerTest, NormalModeGpioSignalingTest) {
  // Initialize the GPIO module, run the signaling procedure, ensure that it
  // concluded that this is a normal mode run.
  StandardGpioMockUdevInterface mock_udev;
  StandardGpioHandler gpio_handler(&mock_udev, false);
  EXPECT_FALSE(gpio_handler.IsTestModeSignaled());
  mock_udev.ExpectAllResourcesDeallocated();
}

TEST(StandardGpioHandlerTest, DeferredInitNormalModeGpioSignalingTest) {
  // Initialize the GPIO module with deferred discovery, run the signaling
  // procedure, ensure that it concluded that this is a normal mode run.
  StandardGpioMockUdevInterface mock_udev;
  StandardGpioHandler gpio_handler(&mock_udev, true);
  EXPECT_FALSE(gpio_handler.IsTestModeSignaled());
  mock_udev.ExpectAllResourcesDeallocated();
}

}  // namespace chromeos_update_engine
