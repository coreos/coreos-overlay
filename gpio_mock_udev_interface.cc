// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/gpio_mock_udev_interface.h"

#include <string>

#include <base/stringprintf.h>
#include <gtest/gtest.h>

#include "gpio_handler_unittest.h"

namespace chromeos_update_engine {

const char* StandardGpioMockUdevInterface::kUdevGpioSubsystem = "gpio";
const char* StandardGpioMockUdevInterface::kUdevGpioChipSysname = "gpiochip*";

const StandardGpioMockUdevInterface::GpioDescriptor
    StandardGpioMockUdevInterface::gpio_descriptors_[kMockGpioIdMax] = {
  { "ID_GPIO_DUTFLAGA", MOCK_DUTFLAGA_GPIO_ID },
  { "ID_GPIO_DUTFLAGB", MOCK_DUTFLAGB_GPIO_ID },
};

const char* StandardGpioMockUdevInterface::enum_gpio_chip_dev_list_[] = {
  "gpiochip" MOCK_GPIO_CHIP_ID,
  NULL
};
const char* StandardGpioMockUdevInterface::enum_dutflaga_gpio_dev_list_[] = {
  "gpio" MOCK_DUTFLAGA_GPIO_ID,
  NULL
};
const char* StandardGpioMockUdevInterface::enum_dutflagb_gpio_dev_list_[] = {
  "gpio" MOCK_DUTFLAGB_GPIO_ID,
  NULL
};

StandardGpioMockUdevInterface::StandardGpioMockUdevInterface()
    : udev_id_(0), udev_enum_id_(0), num_discovered_(0) {
  std::fill_n(used_udev_ids_, MAX_UDEV_OBJECTS, false);
  std::fill_n(used_udev_enum_ids_, MAX_UDEV_ENUM_OBJECTS, false);

  gpio_chip_dev_ = (MockDevice) {
    MOCK_SYSFS_PREFIX "/gpiochip" MOCK_GPIO_CHIP_ID,
    gpio_descriptors_, kMockGpioIdMax, false, false
  };
  dutflaga_gpio_dev_ = (MockDevice) {
    MOCK_SYSFS_PREFIX "/gpio" MOCK_DUTFLAGA_GPIO_ID, NULL, 0, true, false
  };
  dutflagb_gpio_dev_ = (MockDevice) {
    MOCK_SYSFS_PREFIX "/gpio" MOCK_DUTFLAGB_GPIO_ID, NULL, 0, true, false
  };
}

// Device lists are simply null-terminated arrays of strings.
const char* StandardGpioMockUdevInterface::ListEntryGetName(
    struct udev_list_entry* list_entry) {
  const char** mock_list_entry = reinterpret_cast<const char**>(list_entry);
  EXPECT_TRUE(mock_list_entry && *mock_list_entry);
  if (!mock_list_entry)
    return NULL;
  // The mock list entry is the name string itself...
  return *mock_list_entry;
}

struct udev_list_entry* StandardGpioMockUdevInterface::ListEntryGetNext(
    struct udev_list_entry* list_entry) {
  char** mock_list_entry = reinterpret_cast<char**>(list_entry);
  EXPECT_TRUE(mock_list_entry && *mock_list_entry);
  if (!mock_list_entry)
    return NULL;
  // Advance to the next element in the array.
  mock_list_entry++;
  return (*mock_list_entry ?
          reinterpret_cast<struct udev_list_entry*>(mock_list_entry) : NULL);
}

struct udev_device* StandardGpioMockUdevInterface::DeviceNewFromSyspath(
    struct udev* udev, const char* syspath) {
  const size_t udev_id = UdevToId(udev);
  if (udev_id >= MAX_UDEV_OBJECTS)
    return NULL;
  MockDevice* mock_dev;

  // Generate the desired mock device based on the provided syspath.
  if (!strcmp(syspath, enum_gpio_chip_dev_list_[0])) {
    mock_dev = const_cast<MockDevice*>(&gpio_chip_dev_);
  } else if (!strcmp(syspath, enum_dutflaga_gpio_dev_list_[0])) {
    mock_dev = const_cast<MockDevice*>(&dutflaga_gpio_dev_);
  } else if (!strcmp(syspath, enum_dutflagb_gpio_dev_list_[0])) {
    mock_dev = const_cast<MockDevice*>(&dutflagb_gpio_dev_);
  } else {
    ADD_FAILURE();
    return NULL;
  }

  EXPECT_FALSE(mock_dev->is_used);
  if (mock_dev->is_used)
    return NULL;
  mock_dev->is_used = true;

  return reinterpret_cast<struct udev_device*>(mock_dev);
}

const char* StandardGpioMockUdevInterface::DeviceGetPropertyValue(
    struct udev_device* udev_device, const char* key) {
  const MockDevice* mock_dev = UdevDeviceToMock(udev_device);
  EXPECT_TRUE(mock_dev->properties);
  if (!mock_dev->properties)
    return NULL;
  for (size_t i = 0; i < mock_dev->num_properties; i++)
    if (!strcmp(key, mock_dev->properties[i].property))
      return mock_dev->properties[i].value;
  return NULL;
}

const char* StandardGpioMockUdevInterface::DeviceGetSyspath(
    struct udev_device* udev_device) {
  const MockDevice* mock_dev = UdevDeviceToMock(udev_device);
  if (mock_dev->is_gpio)
    num_discovered_++;
  return mock_dev->syspath;
}

void StandardGpioMockUdevInterface::DeviceUnref(
    struct udev_device* udev_device) {
  MockDevice* mock_dev = UdevDeviceToMock(udev_device);
  mock_dev->is_used = false;
}

struct udev_enumerate* StandardGpioMockUdevInterface::EnumerateNew(
    struct udev* udev) {
  const size_t udev_id = UdevToId(udev);
  if (udev_id >= MAX_UDEV_OBJECTS)
    return NULL;
  // A new enumeration "pointer" is cast for an integer identifier.
  EXPECT_LT(udev_enum_id_, MAX_UDEV_ENUM_OBJECTS);
  if (udev_enum_id_ >= MAX_UDEV_ENUM_OBJECTS)
    return NULL;
  used_udev_enum_ids_[udev_enum_id_] = true;
  return reinterpret_cast<struct udev_enumerate*>(++udev_enum_id_);
}

int StandardGpioMockUdevInterface::EnumerateAddMatchSubsystem(
    struct udev_enumerate* udev_enum, const char* subsystem) {
  const size_t udev_enum_id = UdevEnumToId(udev_enum);
  if (udev_enum_id >= MAX_UDEV_ENUM_OBJECTS)
    return -1;

  // Ensure client is correctly requesting the GPIO subsystem.
  EXPECT_STREQ(subsystem, kUdevGpioSubsystem);
  if (strcmp(subsystem, kUdevGpioSubsystem))
    return -1;

  return 0;
}

int StandardGpioMockUdevInterface::EnumerateAddMatchSysname(
    struct udev_enumerate* udev_enum, const char* sysname) {
  const size_t udev_enum_id = UdevEnumToId(udev_enum);
  if (udev_enum_id >= MAX_UDEV_ENUM_OBJECTS)
    return -1;

  // Ensure client is requesting the correct sysnamem, depending on the correct
  // phase in the detection process.
  switch (udev_enum_id) {
    case 0:  // looking for a GPIO chip
      EXPECT_STREQ(sysname, kUdevGpioChipSysname);
      if (strcmp(sysname, kUdevGpioChipSysname))
        return -1;
      break;
    case 1:  // looking for dut_flaga/b
    case 2: {
      const int gpio_id = udev_enum_id - 1;
      const std::string gpio_pattern =
          StringPrintf("*%s", gpio_descriptors_[gpio_id].value);
      EXPECT_STREQ(sysname, gpio_pattern.c_str());
      if (strcmp(sysname, gpio_pattern.c_str()))
        return -1;
      break;
    }
    default:
      ADD_FAILURE();  // can't get here
      return -1;
  }

  return 0;
}

int StandardGpioMockUdevInterface::EnumerateScanDevices(
    struct udev_enumerate* udev_enum) {
  const size_t udev_enum_id = UdevEnumToId(udev_enum);
  if (udev_enum_id >= MAX_UDEV_ENUM_OBJECTS)
    return -1;
  return 0;  // nothing to do, really
}

struct udev_list_entry* StandardGpioMockUdevInterface::EnumerateGetListEntry(
    struct udev_enumerate* udev_enum) {
  const size_t udev_enum_id = UdevEnumToId(udev_enum);
  if (udev_enum_id >= MAX_UDEV_ENUM_OBJECTS)
    return NULL;

  // Return a list of devices corresponding to the provided enumeration.
  switch (udev_enum_id) {
    case 0:
      return reinterpret_cast<struct udev_list_entry*>(
          enum_gpio_chip_dev_list_);
    case 1:
      return reinterpret_cast<struct udev_list_entry*>(
          enum_dutflaga_gpio_dev_list_);
    case 2:
      return reinterpret_cast<struct udev_list_entry*>(
          enum_dutflagb_gpio_dev_list_);
  }

  ADD_FAILURE();  // can't get here
  return NULL;
}

void StandardGpioMockUdevInterface::EnumerateUnref(
    struct udev_enumerate* udev_enum) {
  const size_t udev_enum_id = UdevEnumToId(udev_enum);
  if (udev_enum_id >= MAX_UDEV_ENUM_OBJECTS)
    return;
  used_udev_enum_ids_[udev_enum_id] = false;  // make sure it's freed just once
}

struct udev* StandardGpioMockUdevInterface::New() {
  // The returned "pointer" is cast from an integer identifier.
  EXPECT_LT(udev_id_, MAX_UDEV_OBJECTS);
  if (udev_id_ >= MAX_UDEV_OBJECTS)
    return NULL;
  used_udev_ids_[udev_id_] = true;
  return reinterpret_cast<struct udev*>(++udev_id_);
}

void StandardGpioMockUdevInterface::Unref(struct udev* udev) {
  // Convert to object identifier, ensuring the object has been "allocated".
  const size_t udev_id = UdevToId(udev);
  if (udev_id >= MAX_UDEV_OBJECTS)
    return;
  used_udev_ids_[udev_id] = false;  // make sure it's freed just once
}

void StandardGpioMockUdevInterface::ExpectAllResourcesDeallocated() const {
  // Make sure that all handles were released.
  for (size_t i = 0; i < MAX_UDEV_OBJECTS; i++)
    EXPECT_FALSE(used_udev_ids_[i]);
  for (size_t i = 0; i < MAX_UDEV_ENUM_OBJECTS; i++)
    EXPECT_FALSE(used_udev_enum_ids_[i]);
  EXPECT_FALSE(gpio_chip_dev_.is_used);
  EXPECT_FALSE(dutflaga_gpio_dev_.is_used);
  EXPECT_FALSE(dutflagb_gpio_dev_.is_used);
}

void StandardGpioMockUdevInterface::ExpectDiscoverySuccess() const {
  EXPECT_EQ(num_discovered_, kMockGpioIdMax);
}

void StandardGpioMockUdevInterface::ExpectDiscoveryFail() const {
  EXPECT_LT(num_discovered_, kMockGpioIdMax);
}

StandardGpioMockUdevInterface::MockDevice*
StandardGpioMockUdevInterface::UdevDeviceToMock(
    struct udev_device* udev_dev) const {
  EXPECT_TRUE(udev_dev);
  if (!udev_dev)
    return NULL;
  MockDevice* mock_dev = reinterpret_cast<MockDevice*>(udev_dev);
  EXPECT_TRUE(mock_dev->is_used);
  if (!mock_dev->is_used)
    return NULL;
  return mock_dev;
}

size_t StandardGpioMockUdevInterface::UdevEnumToId(
    struct udev_enumerate* udev_enum) const {
  EXPECT_TRUE(udev_enum);
  size_t udev_enum_id = reinterpret_cast<size_t>(udev_enum) - 1;
  EXPECT_LT(udev_enum_id, MAX_UDEV_ENUM_OBJECTS);
  EXPECT_TRUE(used_udev_enum_ids_[udev_enum_id]);
  if (!(udev_enum && udev_enum_id < MAX_UDEV_ENUM_OBJECTS &&
        used_udev_enum_ids_[udev_enum_id]))
    return MAX_UDEV_ENUM_OBJECTS;
  return udev_enum_id;
}

size_t StandardGpioMockUdevInterface::UdevToId(struct udev* udev) const {
  EXPECT_TRUE(udev);
  size_t udev_id = reinterpret_cast<size_t>(udev) - 1;
  EXPECT_LT(udev_id, MAX_UDEV_OBJECTS);
  EXPECT_TRUE(used_udev_ids_[udev_id]);
  if (!(udev && udev_id < MAX_UDEV_OBJECTS && used_udev_ids_[udev_id]))
    return MAX_UDEV_OBJECTS;
  return udev_id;
}

#define MOCK_GPIO_CHIP1_ID     "200"
#define MOCK_GPIO_CHIP2_ID     "201"

const char* MultiChipGpioMockUdevInterface::enum_gpio_chip_dev_list_[] = {
  "gpiochip" MOCK_GPIO_CHIP1_ID,
  "gpiochip" MOCK_GPIO_CHIP2_ID,
  NULL
};

MultiChipGpioMockUdevInterface::MultiChipGpioMockUdevInterface() {
  gpio_chip1_dev_ = (MockDevice) {
    MOCK_SYSFS_PREFIX "/gpiochip" MOCK_GPIO_CHIP1_ID,
    gpio_descriptors_, kMockGpioIdMax, false, false
  };
  gpio_chip2_dev_ = (MockDevice) {
    MOCK_SYSFS_PREFIX "/gpiochip" MOCK_GPIO_CHIP2_ID,
    gpio_descriptors_, kMockGpioIdMax, false, false
  };
}

struct udev_device* MultiChipGpioMockUdevInterface::DeviceNewFromSyspath(
    struct udev* udev, const char* syspath) {
  const size_t udev_id = UdevToId(udev);
  if (udev_id >= MAX_UDEV_OBJECTS)
    return NULL;
  MockDevice* mock_dev;

  // Generate the desired mock device based on the provided syspath.
  if (!strcmp(syspath, enum_gpio_chip_dev_list_[0])) {
    mock_dev = const_cast<MockDevice*>(&gpio_chip1_dev_);
  } else if (!strcmp(syspath, enum_gpio_chip_dev_list_[1])) {
    mock_dev = const_cast<MockDevice*>(&gpio_chip2_dev_);
  } else if (!strcmp(syspath, enum_dutflaga_gpio_dev_list_[0])) {
    mock_dev = const_cast<MockDevice*>(&dutflaga_gpio_dev_);
  } else if (!strcmp(syspath, enum_dutflagb_gpio_dev_list_[0])) {
    mock_dev = const_cast<MockDevice*>(&dutflagb_gpio_dev_);
  } else {
    ADD_FAILURE();
    return NULL;
  }

  EXPECT_FALSE(mock_dev->is_used);
  if (mock_dev->is_used)
    return NULL;
  mock_dev->is_used = true;

  return reinterpret_cast<struct udev_device*>(mock_dev);
}

struct udev_list_entry* MultiChipGpioMockUdevInterface::EnumerateGetListEntry(
    struct udev_enumerate* udev_enum) {
  const size_t udev_enum_id = UdevEnumToId(udev_enum);

  // Return a list of devices corresponding to the provided enumeration.
  switch (udev_enum_id) {
    case 0:
      return reinterpret_cast<struct udev_list_entry*>(
          enum_gpio_chip_dev_list_);
    case 1:
      return reinterpret_cast<struct udev_list_entry*>(
          enum_dutflaga_gpio_dev_list_);
    case 2:
      return reinterpret_cast<struct udev_list_entry*>(
          enum_dutflagb_gpio_dev_list_);
  }

  ADD_FAILURE();  // can't get here
  return NULL;
}


void FailInitGpioMockUdevInterface::ExpectNumInitAttempts(
    unsigned count) const {
  EXPECT_EQ(num_init_attempts_, count);
}

struct udev* FailInitGpioMockUdevInterface::New() {
  // Increment udev init attempt counter, failing the first attempt.
  num_init_attempts_++;
  return num_init_attempts_ == 1 ? NULL : StandardGpioMockUdevInterface::New();
}

}  // namespace chromeos_update_engine
