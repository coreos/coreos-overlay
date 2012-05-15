// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_GPIO_HANDLER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_GPIO_HANDLER_H__

#include <libudev.h>

#include <string>

#include <base/time.h>

#include "update_engine/file_descriptor.h"
#include "update_engine/udev_interface.h"

namespace chromeos_update_engine {

// An abstract GPIO handler interface. Serves as basic for both concrete and
// mock implementations.
class GpioHandler {
 public:
  GpioHandler() {}
  virtual ~GpioHandler() {};  // ensure virtual destruction

  // Returns true iff GPIOs have been used to signal an automated test case.
  // This call may trigger a (deferred) GPIO discovery step prior to engaging in
  // the signaling protocol; if discovery did not reveal GPIO devices, or the
  // protocol has terminated prematurely, it will conservatively default to
  // false.
  virtual bool IsTestModeSignaled() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpioHandler);
};


// Concrete implementation of GPIO signal handling. Currently, it only utilizes
// the two Chromebook-specific GPIOs (aka 'dut_flaga' and 'dut_flagb') in
// deciding whether a lab test mode has been signaled.  Internal logic includes
// detection, setup and reading from / writing to GPIOs. Detection is done via
// libudev calls.  This class should be instantiated at most once to avoid race
// conditions in communicating over GPIO signals; instantiating a second
// object will actually cause a runtime error.
class StandardGpioHandler : public GpioHandler {
 public:
  // This constructor accepts a udev interface |udev_iface|. The value of
  // |is_defer_discovery| determines whether GPIO discovery should be attempted
  // right away (false) or done lazily, when necessitated by other calls (true).
  StandardGpioHandler(UdevInterface* udev_iface, bool is_defer_discovery);

  // Free all resources, allow to reinstantiate.
  virtual ~StandardGpioHandler();

  // Returns true iff GPIOs have been used to signal an automated test case. The
  // check is performed at most once and the result is cached and returned on
  // subsequent calls, unless |is_force| is true. This call may trigger a
  // delayed GPIO discovery prior to engaging in the signaling protocol; if the
  // delay period has not elapsed, it will conservatively default to false.
  virtual bool IsTestModeSignaled();

 private:
  // GPIO identifiers, currently includes only the two dutflags.
  enum GpioId {
    kGpioIdDutflaga = 0,
    kGpioIdDutflagb,
    kGpioIdMax  // marker, do not remove!
  };

  // GPIO direction specifier.
  enum GpioDir {
    kGpioDirIn = 0,
    kGpioDirOut,
    kGpioDirMax  // marker, do not remove!
  };

  // GPIO value.
  enum GpioVal {
    kGpioValUp = 0,
    kGpioValDown,
    kGpioValMax  // marker, do not remove!
  };

  // GPIO definition data.
  struct GpioDef {
    const char* name;           // referential name of this GPIO
    const char* udev_property;  // udev property containing the device id
  };

  // GPIO runtime control structure.
  struct Gpio {
    std::string descriptor;  // unique GPIO descriptor
    std::string dev_path;    // sysfs device name
  };

  // Various constants.
  static const int kServoInputResponseTimeoutInSecs = 3;
  static const int kServoInputNumChecksPerSec = 5;
  static const int kServoOutputResponseWaitInSecs = 2;

  // GPIO value/direction conversion tables.
  static const char* gpio_dirs_[kGpioDirMax];
  static const char* gpio_vals_[kGpioValMax];

  // GPIO definitions.
  static const GpioDef gpio_defs_[kGpioIdMax];

  // Udev device enumeration helper classes. First here is an interface
  // definition, which provides callbacks for enumeration setup and processing.
  class UdevEnumHelper {
   public:
    UdevEnumHelper(StandardGpioHandler* gpio_handler)
        : gpio_handler_(gpio_handler) {}

    // Setup the enumeration filters.
    virtual bool SetupEnumFilters(udev_enumerate* udev_enum) = 0;

    // Processes an enumerated device. Returns true upon success, false
    // otherwise.
    virtual bool ProcessDev(udev_device* dev) = 0;

    // Finalize the enumeration.
    virtual bool Finalize() = 0;

   protected:
    StandardGpioHandler* gpio_handler_;

   private:
    DISALLOW_COPY_AND_ASSIGN(UdevEnumHelper);
  };

  // Specialized udev enumerate helper for extracting GPIO descriptors from the
  // GPIO chip device.
  class GpioChipUdevEnumHelper : public UdevEnumHelper {
   public:
    GpioChipUdevEnumHelper(StandardGpioHandler* gpio_handler)
        : UdevEnumHelper(gpio_handler), num_gpio_chips_(0) {}
    virtual bool SetupEnumFilters(udev_enumerate* udev_enum);
    virtual bool ProcessDev(udev_device* dev);
    virtual bool Finalize();

   private:
    // Records the number of times a GPIO chip has been enumerated (should not
    // exceed 1).
    int num_gpio_chips_;

    DISALLOW_COPY_AND_ASSIGN(GpioChipUdevEnumHelper);
  };

  // Specialized udev enumerate helper for extracting a sysfs device path from a
  // GPIO device.
  class GpioUdevEnumHelper : public UdevEnumHelper {
   public:
    GpioUdevEnumHelper(StandardGpioHandler* gpio_handler, GpioId id)
        : UdevEnumHelper(gpio_handler), num_gpios_(0), id_(id) {}
    virtual bool SetupEnumFilters(udev_enumerate* udev_enum);
    virtual bool ProcessDev(udev_device* dev);
    virtual bool Finalize();

   private:
    // Records the number of times a GPIO has been enumerated with a given
    // descriptor (should not exceed 1).
    int num_gpios_;

    // The enumerated GPIO identifier.
    GpioId id_;

    DISALLOW_COPY_AND_ASSIGN(GpioUdevEnumHelper);
  };

  // Attempt GPIO discovery, at most once. Returns true if discovery process was
  // successfully completed or already attempted, false otherwise.
  bool DiscoverGpios();

  // An initialization helper performing udev enumeration. |enum_helper|
  // implements an enumeration initialization and processing methods. Returns
  // true upon success, false otherwise.
  bool InitUdevEnum(struct udev* udev, UdevEnumHelper* enum_helper);

  // Assigns a copy of the device name of GPIO |id| to |dev_path_p|. Assumes
  // initialization. Returns true upon success, false otherwise.
  bool GetGpioDevName(GpioId id, std::string* dev_path_p);

  // Dynamic counter for the number of instances this class has. Used to enforce
  // that no more than one instance created. Thread-unsafe.
  static unsigned num_instances_;

  // GPIO control structures.
  Gpio gpios_[kGpioIdMax];

  // Udev interface.
  UdevInterface* const udev_iface_;

  // Indicates whether GPIO discovery was performed.
  bool is_discovery_attempted_;

  DISALLOW_COPY_AND_ASSIGN(StandardGpioHandler);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_GPIO_HANDLER_H__
