// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "update_engine/gpio_handler.h"
#include "update_engine/gpio_mock_file_descriptor.h"
#include "update_engine/gpio_mock_udev_interface.h"

namespace chromeos_update_engine {

class StandardGpioHandlerTest : public ::testing::Test {};

TEST(StandardGpioHandlerTest, NormalInitTest) {
  // Ensure that initialization of the GPIO module works as expected, and that
  // all udev resources are deallocated afterwards.  The mock file descriptor is
  // not to be used.
  StandardGpioMockUdevInterface mock_udev;
  TestModeGpioMockFileDescriptor
      mock_file_descriptor(base::TimeDelta::FromSeconds(1));
  StandardGpioHandler gpio_hander(&mock_udev, &mock_file_descriptor,
                                  false, false);
  mock_udev.ExpectAllResourcesDeallocated();
  mock_udev.ExpectDiscoverySuccess();
  mock_file_descriptor.ExpectNumOpenAttempted(0);
}

TEST(StandardGpioHandlerTest, MultiGpioChipInitTest) {
  // Attempt GPIO discovery with a udev mock that returns two GPIO chip devices.
  // It should fail, of course.  The mock file descriptor is not to be used.
  MultiChipGpioMockUdevInterface mock_udev;
  TestModeGpioMockFileDescriptor
      mock_file_descriptor(base::TimeDelta::FromSeconds(1));
  StandardGpioHandler gpio_handler(&mock_udev, &mock_file_descriptor,
                                   false, false);
  mock_udev.ExpectAllResourcesDeallocated();
  mock_udev.ExpectDiscoveryFail();
  mock_file_descriptor.ExpectNumOpenAttempted(0);
}

TEST(StandardGpioHandlerTest, FailedFirstGpioInitTest) {
  // Attempt GPIO discovery with a udev mock that fails the initialization on
  // the first attempt, then check for test mode. Ensure that (a) discovery is
  // not attempted a second time, and (b) test mode check returns false (the
  // default) without attempting to use GPIO signals.
  FailInitGpioMockUdevInterface mock_udev;
  TestModeGpioMockFileDescriptor
      mock_file_descriptor(base::TimeDelta::FromSeconds(1));
  StandardGpioHandler gpio_handler(&mock_udev, &mock_file_descriptor,
                                   false, false);
  EXPECT_FALSE(gpio_handler.IsTestModeSignaled());
  mock_udev.ExpectAllResourcesDeallocated();
  mock_udev.ExpectDiscoveryFail();
  mock_udev.ExpectNumInitAttempts(1);
  mock_file_descriptor.ExpectNumOpenAttempted(0);
}

TEST(StandardGpioHandlerTest, TestModeGpioSignalingTest) {
  // Initialize the GPIO module and test for successful completion of the test
  // signaling protocol.
  StandardGpioMockUdevInterface mock_udev;
  TestModeGpioMockFileDescriptor
      mock_file_descriptor(base::TimeDelta::FromSeconds(1));
  StandardGpioHandler gpio_handler(&mock_udev, &mock_file_descriptor,
                                   false, false);
  EXPECT_TRUE(gpio_handler.IsTestModeSignaled());
  mock_udev.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllGpiosRestoredToDefault();
}

TEST(StandardGpioHandlerTest, DeferredInitTestModeGpioSignalingTest) {
  // Initialize the GPIO module with deferred initialization, test for
  // successful completion of the test signaling protocol.
  StandardGpioMockUdevInterface mock_udev;
  TestModeGpioMockFileDescriptor
      mock_file_descriptor(base::TimeDelta::FromSeconds(1));
  StandardGpioHandler gpio_handler(&mock_udev, &mock_file_descriptor,
                                   true, false);
  EXPECT_TRUE(gpio_handler.IsTestModeSignaled());
  mock_udev.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllGpiosRestoredToDefault();
}

TEST(StandardGpioHandlerTest, TestModeGpioSignalingTwiceTest) {
  // Initialize the GPIO module and query for test signal twice (uncached); the
  // first query should succeed whereas the second should fail.
  StandardGpioMockUdevInterface mock_udev;
  TestModeGpioMockFileDescriptor
      mock_file_descriptor(base::TimeDelta::FromSeconds(1));
  StandardGpioHandler gpio_handler(&mock_udev, &mock_file_descriptor,
                                   false, false);
  EXPECT_TRUE(gpio_handler.IsTestModeSignaled());
  EXPECT_FALSE(gpio_handler.IsTestModeSignaled());
  mock_udev.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllGpiosRestoredToDefault();
}

TEST(StandardGpioHandlerTest, TestModeGpioSignalingTwiceCachedTest) {
  // Initialize the GPIO module and query for test signal twice (cached); both
  // queries should succeed.
  StandardGpioMockUdevInterface mock_udev;
  TestModeGpioMockFileDescriptor
      mock_file_descriptor(base::TimeDelta::FromSeconds(1));
  StandardGpioHandler gpio_handler(&mock_udev, &mock_file_descriptor,
                                   false, true);
  EXPECT_TRUE(gpio_handler.IsTestModeSignaled());
  EXPECT_TRUE(gpio_handler.IsTestModeSignaled());
  mock_udev.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllGpiosRestoredToDefault();
}

TEST(StandardGpioHandlerTest, NormalModeGpioSignalingTest) {
  // Initialize the GPIO module, run the signaling procedure, ensure that it
  // concluded that this is a normal mode run.
  StandardGpioMockUdevInterface mock_udev;
  NormalModeGpioMockFileDescriptor mock_file_descriptor;
  StandardGpioHandler gpio_handler(&mock_udev, &mock_file_descriptor,
                                   false, false);
  EXPECT_FALSE(gpio_handler.IsTestModeSignaled());
  mock_udev.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllGpiosRestoredToDefault();
}

TEST(StandardGpioHandlerTest, NonPulledUpNormalModeGpioSignalingTest) {
  // Initialize the GPIO module with a non-pulled up mock (which means the it
  // returns a different default signal), run the signaling procedure, ensure
  // that it concluded that this is a normal mode run.
  StandardGpioMockUdevInterface mock_udev;
  NonPulledUpNormalModeGpioMockFileDescriptor mock_file_descriptor;
  StandardGpioHandler gpio_handler(&mock_udev, &mock_file_descriptor,
                                   false, false);
  EXPECT_FALSE(gpio_handler.IsTestModeSignaled());
  mock_udev.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllGpiosRestoredToDefault();
}

TEST(StandardGpioHandlerTest, DeferredInitNormalModeGpioSignalingTest) {
  // Initialize the GPIO module with deferred discovery, run the signaling
  // procedure, ensure that it concluded that this is a normal mode run.
  StandardGpioMockUdevInterface mock_udev;
  NormalModeGpioMockFileDescriptor mock_file_descriptor;
  StandardGpioHandler gpio_handler(&mock_udev, &mock_file_descriptor,
                                   true, false);
  EXPECT_FALSE(gpio_handler.IsTestModeSignaled());
  mock_udev.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllGpiosRestoredToDefault();
}

TEST(StandardGpioHandlerTest, FlipInputDirErrorNormalModeGpioSignalingTest) {
  // Test the GPIO module with a mock that simulates a GPIO sysfs race/hack,
  // which causes the input GPIO to flip direction. Ensure that it concludes
  // that this is a normal mode run.
  StandardGpioMockUdevInterface mock_udev;
  ErrorNormalModeGpioMockFileDescriptor
      mock_file_descriptor(
          base::TimeDelta::FromSeconds(1),
          ErrorNormalModeGpioMockFileDescriptor::kGpioErrorFlipInputDir);
  StandardGpioHandler gpio_handler(&mock_udev, &mock_file_descriptor,
                                   false, false);
  EXPECT_FALSE(gpio_handler.IsTestModeSignaled());
  mock_udev.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllGpiosRestoredToDefault();
}

TEST(StandardGpioHandlerTest, ReadInvalidValErrorNormalModeGpioSignalingTest) {
  // Test the GPIO module with a mock that simulates an invalid value reading
  // from a GPIO device. Ensure that it concludes that this is a normal mode
  // run.
  StandardGpioMockUdevInterface mock_udev;
  ErrorNormalModeGpioMockFileDescriptor
      mock_file_descriptor(
          base::TimeDelta::FromSeconds(1),
          ErrorNormalModeGpioMockFileDescriptor::kGpioErrorReadInvalidVal);
  StandardGpioHandler gpio_handler(&mock_udev, &mock_file_descriptor,
                                   false, false);
  EXPECT_FALSE(gpio_handler.IsTestModeSignaled());
  mock_udev.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllGpiosRestoredToDefault();
}

TEST(StandardGpioHandlerTest, ReadInvalidDirErrorNormalModeGpioSignalingTest) {
  // Test the GPIO module with a mock that simulates an invalid value reading
  // from a GPIO device. Ensure that it concludes that this is a normal mode
  // run.
  StandardGpioMockUdevInterface mock_udev;
  ErrorNormalModeGpioMockFileDescriptor
      mock_file_descriptor(
          base::TimeDelta::FromSeconds(1),
          ErrorNormalModeGpioMockFileDescriptor::kGpioErrorReadInvalidDir);
  StandardGpioHandler gpio_handler(&mock_udev, &mock_file_descriptor,
                                   false, false);
  EXPECT_FALSE(gpio_handler.IsTestModeSignaled());
  mock_udev.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllGpiosRestoredToDefault();
}

TEST(StandardGpioHandlerTest, FailFileOpenErrorNormalModeGpioSignalingTest) {
  // Test the GPIO module with a mock that simulates an invalid value reading
  // from a GPIO device. Ensure that it concludes that this is a normal mode
  // run.
  StandardGpioMockUdevInterface mock_udev;
  ErrorNormalModeGpioMockFileDescriptor
      mock_file_descriptor(
          base::TimeDelta::FromSeconds(1),
          ErrorNormalModeGpioMockFileDescriptor::kGpioErrorFailFileOpen);
  StandardGpioHandler gpio_handler(&mock_udev, &mock_file_descriptor,
                                   false, false);
  EXPECT_FALSE(gpio_handler.IsTestModeSignaled());
  mock_udev.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllGpiosRestoredToDefault();
}

TEST(StandardGpioHandlerTest, FailFileReadErrorNormalModeGpioSignalingTest) {
  // Test the GPIO module with a mock that simulates an invalid value reading
  // from a GPIO device. Ensure that it concludes that this is a normal mode
  // run.
  StandardGpioMockUdevInterface mock_udev;
  ErrorNormalModeGpioMockFileDescriptor
      mock_file_descriptor(
          base::TimeDelta::FromSeconds(1),
          ErrorNormalModeGpioMockFileDescriptor::kGpioErrorFailFileRead);
  StandardGpioHandler gpio_handler(&mock_udev, &mock_file_descriptor,
                                   false, false);
  EXPECT_FALSE(gpio_handler.IsTestModeSignaled());
  mock_udev.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllGpiosRestoredToDefault();
}

TEST(StandardGpioHandlerTest, FailFileWriteErrorNormalModeGpioSignalingTest) {
  // Test the GPIO module with a mock that simulates an invalid value reading
  // from a GPIO device. Ensure that it concludes that this is a normal mode
  // run.
  StandardGpioMockUdevInterface mock_udev;
  ErrorNormalModeGpioMockFileDescriptor
      mock_file_descriptor(
          base::TimeDelta::FromSeconds(1),
          ErrorNormalModeGpioMockFileDescriptor::kGpioErrorFailFileWrite);
  StandardGpioHandler gpio_handler(&mock_udev, &mock_file_descriptor,
                                   false, false);
  EXPECT_FALSE(gpio_handler.IsTestModeSignaled());
  mock_udev.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllGpiosRestoredToDefault();
}

TEST(StandardGpioHandlerTest, FailFileCloseErrorNormalModeGpioSignalingTest) {
  // Test the GPIO module with a mock that simulates an invalid value reading
  // from a GPIO device. Ensure that it concludes that this is a normal mode
  // run.
  StandardGpioMockUdevInterface mock_udev;
  ErrorNormalModeGpioMockFileDescriptor
      mock_file_descriptor(
          base::TimeDelta::FromSeconds(1),
          ErrorNormalModeGpioMockFileDescriptor::kGpioErrorFailFileClose);
  StandardGpioHandler gpio_handler(&mock_udev, &mock_file_descriptor,
                                   false, false);
  EXPECT_FALSE(gpio_handler.IsTestModeSignaled());
  mock_udev.ExpectAllResourcesDeallocated();
  mock_file_descriptor.ExpectAllResourcesDeallocated();
  // Don't test GPIO status restored; since closing of sysfs files fails, all
  // bets are off.
}

}  // namespace chromeos_update_engine
