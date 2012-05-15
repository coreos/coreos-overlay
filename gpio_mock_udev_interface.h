// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_GPIO_MOCK_UDEV_INTERFACE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_GPIO_MOCK_UDEV_INTERFACE_H__

#include "update_engine/gpio_handler.h"
#include "update_engine/udev_interface.h"

// A set of mock udev interfaces for unit testing of GPIO handler.

// Constant defining number of allowable mock udev objects.
#define MAX_UDEV_OBJECTS      1
#define MAX_UDEV_ENUM_OBJECTS 3

namespace chromeos_update_engine {

// An abstract class providing some diagnostic methods for testing.
class GpioMockUdevInterface : public UdevInterface {
 public:
  // Asserts that all resources have been properly deallocated.
  virtual void ExpectAllResourcesDeallocated() const = 0;
  // Asserts the the udev client has successfully discovered the syspath of the
  // GPIO signals.
  virtual void ExpectDiscoverySuccess() const = 0;
  // Asserts the opposite.
  virtual void ExpectDiscoveryFail() const = 0;
};

class StandardGpioMockUdevInterface : public GpioMockUdevInterface {
 public:
  // Default constructor.
  StandardGpioMockUdevInterface();

  // Inherited interface methods.
  virtual const char* ListEntryGetName(struct udev_list_entry* list_entry);
  virtual udev_list_entry* ListEntryGetNext(struct udev_list_entry* list_entry);

  virtual struct udev_device* DeviceNewFromSyspath(struct udev* udev,
                                                   const char* syspath);
  virtual const char* DeviceGetPropertyValue(struct udev_device* udev_device,
                                             const char* key);
  virtual const char* DeviceGetSyspath(struct udev_device* udev_device);
  virtual void DeviceUnref(struct udev_device* udev_device);

  virtual struct udev_enumerate* EnumerateNew(struct udev* udev);
  virtual int EnumerateAddMatchSubsystem(struct udev_enumerate* udev_enum,
                                         const char* subsystem);
  virtual int EnumerateAddMatchSysname(struct udev_enumerate* udev_enum,
                                       const char* sysname);
  virtual int EnumerateScanDevices(struct udev_enumerate* udev_enum);
  virtual struct udev_list_entry* EnumerateGetListEntry(
      struct udev_enumerate* udev_enum);
  virtual void EnumerateUnref(struct udev_enumerate* udev_enum);

  virtual struct udev* New();
  virtual void Unref(struct udev* udev);

  virtual void ExpectAllResourcesDeallocated() const;
  virtual void ExpectDiscoverySuccess() const;
  virtual void ExpectDiscoveryFail() const;

 protected:
  // Some constants.
  static const char* kUdevGpioSubsystem;
  static const char* kUdevGpioChipSysname;
  static const char* kMockGpioSysfsRoot;

  // GPIO descriptor lookup.
  struct GpioDescriptor {
    const char* property;
    const char* value;
  };
  static const GpioDescriptor gpio_descriptors_[];

  // Numeric identifiers for new udev and enumerate objects.
  size_t udev_id_;
  size_t udev_enum_id_;

  // Null-terminated lists of devices returned by various enumerations.
  static const char* enum_gpio_chip_dev_list_[];
  static const char* enum_dutflaga_gpio_dev_list_[];
  static const char* enum_dutflagb_gpio_dev_list_[];

  // Mock devices to be used during GPIO discovery.  These contain the syspath
  // and a set of properties that the device may contain.
  struct MockDevice {
    const char* syspath;
    const GpioDescriptor* properties;
    size_t num_properties;
    bool is_gpio;
    bool is_used;
  };
  MockDevice gpio_chip_dev_;
  MockDevice dutflaga_gpio_dev_;
  MockDevice dutflagb_gpio_dev_;

  // Tracking active mock object handles.
  bool used_udev_ids_[MAX_UDEV_OBJECTS];
  bool used_udev_enum_ids_[MAX_UDEV_ENUM_OBJECTS];

  // Track discovery progress of GPIO signals.
  unsigned num_discovered_;

  // Convert mock udev handles into internal handles, with sanity checks.
  MockDevice* UdevDeviceToMock(struct udev_device* udev_dev) const;
  size_t UdevEnumToId(struct udev_enumerate* udev_enum) const;
  size_t UdevToId(struct udev* udev) const;
};

class MultiChipGpioMockUdevInterface : public StandardGpioMockUdevInterface {
 public:
  // Default constructor.
  MultiChipGpioMockUdevInterface();

 protected:
  virtual struct udev_device* DeviceNewFromSyspath(struct udev* udev,
                                                   const char* syspath);
  virtual struct udev_list_entry* EnumerateGetListEntry(
      struct udev_enumerate* udev_enum);

  // Null-terminated lists of devices returned by various enumerations.
  static const char* enum_gpio_chip_dev_list_[];

  // Mock devices to be used during GPIO discovery.  These contain the syspath
  // and a set of properties that the device may contain.
  MockDevice gpio_chip1_dev_;
  MockDevice gpio_chip2_dev_;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_GPIO_MOCK_UDEV_INTERFACE_H__
