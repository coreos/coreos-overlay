// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_GPIO_HANDLER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_GPIO_HANDLER_H__

#include <libudev.h>

#include <string>

#include <base/basictypes.h>

namespace chromeos_update_engine {

// Handles GPIO signals, which are used for indicating a lab test environment.
// This class detects, reads and decides whether a test scenario is in effect,
// which can be used by client code to trigger test-specific behavior.  This
// class has only static members/methods and cannot be instantiated.
class GpioHandler {
 public:
  // Returns true iff GPIOs have been signaled to indicate an automated test
  // case. This triggers a discovery and reading of the dutflaga/b GPIOs.
  static bool IsGpioSignalingTest();

 private:
  // This class cannot be instantiated.
  GpioHandler() {}

  // Enumerator for dutflag GPIOs.
  enum DutflagGpioId {
    kDutflagaGpio,
    kDutflagbGpio,
  };

  // Gets the fully qualified sysfs name of a dutflag device.  |udev| is a live
  // libudev instance; |gpio_dutflag_str| is the identifier for the requested
  // dutflag GPIO. The output is stored in the string pointed to by
  // |dutflag_dev_name_p|.  Returns true upon success, false otherwise.
  static bool GetDutflagGpioDevName(struct udev* udev,
                                    const std::string& gpio_dutflag_str,
                                    const char** dutflag_dev_name_p);

  // Gets the dut_flaga/b GPIO device names and copies them into the two string
  // arguments, respectively. A string pointer may be null, in which case
  // discovery will not be attempted for the corresponding device. The function
  // caches these strings, which are assumed to be hardware constants. Returns
  // true upon success, false otherwise.
  static bool GetDutflagGpioDevNames(std::string* dutflaga_dev_name_p,
                                     std::string* dutflagb_dev_name_p);

  // Writes the dut_flaga GPIO status into its argument, where true/false stand
  // for "on"/"off", respectively. Returns true upon success, false otherwise
  // (in which case no value is written to |status|).
  static bool GetDutflagaGpio(bool* status);

  // Reads the value of a dut_flag GPIO |id| and stores it in |status_p|.
  // Returns true upon success, false otherwise (which also means that the GPIO
  // value was not stored and should not be used).
  static bool GetDutflagGpioStatus(DutflagGpioId id, bool* status_p);

  // Dutflaga/b GPIO device names.
  static const char* dutflaga_dev_name_;
  static const char* dutflagb_dev_name_;

  DISALLOW_COPY_AND_ASSIGN(GpioHandler);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_GPIO_HANDLER_H__
