// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpio_handler.h"

#include <base/memory/scoped_ptr.h>
#include <base/string_util.h>
#include <base/stringprintf.h>

#include "update_engine/file_descriptor.h"

using base::Time;
using base::TimeDelta;
using std::string;

using namespace chromeos_update_engine;

namespace chromeos_update_engine {

const char* StandardGpioHandler::gpio_dirs_[kGpioDirMax] = {
  "in",   // kGpioDirIn
  "out",  // kGpioDirOut
};

const char* StandardGpioHandler::gpio_vals_[kGpioValMax] = {
  "1",  // kGpioValUp
  "0",  // kGpioValDown
};

const StandardGpioHandler::GpioDef
StandardGpioHandler::gpio_defs_[kGpioIdMax] = {
  { "dutflaga", "ID_GPIO_DUTFLAGA" },  // kGpioDutflaga
  { "dutflagb", "ID_GPIO_DUTFLAGB" },  // kGpioDutflagb
};

unsigned StandardGpioHandler::num_instances_ = 0;


StandardGpioHandler::StandardGpioHandler(UdevInterface* udev_iface,
                                         bool is_defer_discovery)
    : udev_iface_(udev_iface),
      is_discovery_attempted_(false) {
  CHECK(udev_iface);

  // Ensure there's only one instance of this class.
  CHECK_EQ(num_instances_, static_cast<unsigned>(0));
  num_instances_++;

  // If GPIO discovery not deferred, do it.
  if (!(is_defer_discovery || DiscoverGpios())) {
    LOG(WARNING) << "GPIO discovery failed";
  }
}

StandardGpioHandler::~StandardGpioHandler() {
  num_instances_--;
}

// TODO(garnold) currently, this function always returns false and avoids the
// GPIO signaling protocol altogether; to be extended later.
bool StandardGpioHandler::IsTestModeSignaled() {
  // Attempt GPIO discovery.
  if (!DiscoverGpios()) {
    LOG(WARNING) << "GPIO discovery failed";
  }

  return false;
}

bool StandardGpioHandler::GpioChipUdevEnumHelper::SetupEnumFilters(
    udev_enumerate* udev_enum) {
  CHECK(udev_enum);

  return !(gpio_handler_->udev_iface_->EnumerateAddMatchSubsystem(
               udev_enum, "gpio") ||
           gpio_handler_->udev_iface_->EnumerateAddMatchSysname(
               udev_enum, "gpiochip*"));
}

bool StandardGpioHandler::GpioChipUdevEnumHelper::ProcessDev(udev_device* dev) {
  CHECK(dev);

  // Ensure we did not encounter more than one chip.
  if (num_gpio_chips_++) {
    LOG(ERROR) << "enumerated multiple GPIO chips";
    return false;
  }

  // Obtain GPIO descriptors.
  for (int id = 0; id < kGpioIdMax; id++) {
    const GpioDef* gpio_def = &gpio_defs_[id];
    const char* descriptor =
        gpio_handler_->udev_iface_->DeviceGetPropertyValue(
            dev, gpio_def->udev_property);
    if (!descriptor) {
      LOG(ERROR) << "could not obtain " << gpio_def->name
                 << " descriptor using property " << gpio_def->udev_property;
      return false;
    }
    gpio_handler_->gpios_[id].descriptor = descriptor;
  }

  return true;
}

bool StandardGpioHandler::GpioChipUdevEnumHelper::Finalize() {
  if (num_gpio_chips_ != 1) {
    LOG(ERROR) << "could not enumerate a GPIO chip";
    return false;
  }
  return true;
}

bool StandardGpioHandler::GpioUdevEnumHelper::SetupEnumFilters(
    udev_enumerate* udev_enum) {
  CHECK(udev_enum);
  const string gpio_pattern =
      string("*").append(gpio_handler_->gpios_[id_].descriptor);
  return !(
      gpio_handler_->udev_iface_->EnumerateAddMatchSubsystem(
          udev_enum, "gpio") ||
      gpio_handler_->udev_iface_->EnumerateAddMatchSysname(
          udev_enum, gpio_pattern.c_str()));
}

bool StandardGpioHandler::GpioUdevEnumHelper::ProcessDev(udev_device* dev) {
  CHECK(dev);

  // Ensure we did not encounter more than one GPIO device.
  if (num_gpios_++) {
    LOG(ERROR) << "enumerated multiple GPIO devices for a given descriptor";
    return false;
  }

  // Obtain GPIO device sysfs path.
  const char* dev_path = gpio_handler_->udev_iface_->DeviceGetSyspath(dev);
  if (!dev_path) {
    LOG(ERROR) << "failed to obtain device syspath for GPIO "
               << gpio_defs_[id_].name;
    return false;
  }
  gpio_handler_->gpios_[id_].dev_path = dev_path;

  LOG(INFO) << "obtained device syspath: " << gpio_defs_[id_].name << " -> "
            << gpio_handler_->gpios_[id_].dev_path;
  return true;
}

bool StandardGpioHandler::GpioUdevEnumHelper::Finalize() {
  if (num_gpios_ != 1) {
    LOG(ERROR) << "could not enumerate GPIO device " << gpio_defs_[id_].name;
    return false;
  }
  return true;
}


bool StandardGpioHandler::InitUdevEnum(struct udev* udev,
                                       UdevEnumHelper* enum_helper) {
  // Obtain a udev enumerate object.
  struct udev_enumerate* udev_enum;
  if (!(udev_enum = udev_iface_->EnumerateNew(udev))) {
    LOG(ERROR) << "failed to obtain udev enumerate context";
    return false;
  }

  // Assign enumerate object to closer.
  scoped_ptr<UdevInterface::UdevEnumerateCloser>
      udev_enum_closer(udev_iface_->NewUdevEnumerateCloser(&udev_enum));

  // Setup enumeration filters.
  if (!enum_helper->SetupEnumFilters(udev_enum)) {
    LOG(ERROR) << "failed to setup udev enumerate filters";
    return false;
  }

  // Scan for matching devices.
  if (udev_iface_->EnumerateScanDevices(udev_enum)) {
    LOG(ERROR) << "udev enumerate scan failed";
    return false;
  }

  // Iterate over matching devices.
  struct udev_list_entry* list_entry;
  for (list_entry = udev_iface_->EnumerateGetListEntry(udev_enum);
       list_entry; list_entry = udev_iface_->ListEntryGetNext(list_entry)) {
    // Obtain device name.
    const char* dev_path = udev_iface_->ListEntryGetName(list_entry);
    if (!dev_path) {
      LOG(ERROR) << "enumerated device has a null name string";
      return false;
    }

    // Obtain device object.
    struct udev_device* dev = udev_iface_->DeviceNewFromSyspath(udev, dev_path);
    if (!dev) {
      LOG(ERROR) << "obtained a null device object for enumerated device";
      return false;
    }
    scoped_ptr<UdevInterface::UdevDeviceCloser>
        dev_closer(udev_iface_->NewUdevDeviceCloser(&dev));

    if (!enum_helper->ProcessDev(dev))
      return false;
  }

  // Make sure postconditions were met.
  return enum_helper->Finalize();
}

bool StandardGpioHandler::DiscoverGpios() {
  if (is_discovery_attempted_)
    return true;

  is_discovery_attempted_ = true;

  // Obtain libudev instance and attach to a dedicated closer.
  struct udev* udev;
  if (!(udev = udev_iface_->New())) {
    LOG(ERROR) << "failed to obtain libudev instance";
    return false;
  }
  scoped_ptr<UdevInterface::UdevCloser>
      udev_closer(udev_iface_->NewUdevCloser(&udev));

  // Enumerate GPIO chips, scanning for GPIO descriptors.
  GpioChipUdevEnumHelper chip_enum_helper(this);
  if (!InitUdevEnum(udev, &chip_enum_helper)) {
    LOG(ERROR) << "enumeration error, aborting GPIO discovery";
    return false;
  }

  // Obtain device names for all discovered GPIOs, reusing the udev instance.
  for (int id = 0; id < kGpioIdMax; id++) {
    GpioUdevEnumHelper gpio_enum_helper(this, static_cast<GpioId>(id));
    if (!InitUdevEnum(udev, &gpio_enum_helper)) {
      LOG(ERROR) << "enumeration error, aborting GPIO discovery";
      return false;
    }
  }

  return true;
}

bool StandardGpioHandler::GetGpioDevName(StandardGpioHandler::GpioId id,
                                         string* dev_path_p) {
  CHECK(id >= 0 && id < kGpioIdMax && dev_path_p);

  *dev_path_p = gpios_[id].dev_path;
  return true;
}

}  // namespace chromeos_update_engine
