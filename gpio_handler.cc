// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpio_handler.h"

#include <base/memory/scoped_ptr.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <base/time.h>
#include <glib.h>
#include <fcntl.h>

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
  { "dutflaga", "ID_GPIO_DUTFLAGA" },  // kGpioIdDutflaga
  { "dutflagb", "ID_GPIO_DUTFLAGB" },  // kGpioIdDutflagb
};

unsigned StandardGpioHandler::num_instances_ = 0;


StandardGpioHandler::StandardGpioHandler(UdevInterface* udev_iface,
                                         FileDescriptor* fd,
                                         bool is_defer_discovery,
                                         bool is_cache_test_mode)
    : udev_iface_(udev_iface),
      fd_(fd),
      is_cache_test_mode_(is_cache_test_mode),
      is_discovery_attempted_(false),
      is_discovery_successful_(false) {
  CHECK(udev_iface && fd);

  // Ensure there's only one instance of this class.
  CHECK_EQ(num_instances_, static_cast<unsigned>(0));
  num_instances_++;

  // Reset test signal flags.
  ResetTestModeSignalingFlags();

  // If GPIO discovery not deferred, do it.
  if (!is_defer_discovery)
    DiscoverGpios();
}

StandardGpioHandler::~StandardGpioHandler() {
  num_instances_--;
}

bool StandardGpioHandler::IsTestModeSignaled() {
  bool is_returning_cached = false;  // for logging purposes

  // Attempt GPIO discovery first.
  if (DiscoverGpios()) {
    // Force a check if so requested.
    if (!is_cache_test_mode_)
      ResetTestModeSignalingFlags();

    is_returning_cached = !is_first_check_;  // for logging purposes
    if (is_first_check_) {
      is_first_check_ = false;
      DoTestModeSignalingProtocol();
    }
  }

  LOG(INFO) << "result: " << (is_test_mode_ ? "test" : "normal") << " mode"
            << (is_returning_cached ? " (cached)" : "")
            << (is_handshake_completed_ ? "" : " (default)");
  return is_test_mode_;
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

StandardGpioHandler::GpioDirResetter::GpioDirResetter(
    StandardGpioHandler* handler, GpioId id, GpioDir dir) :
    do_reset_(false), handler_(handler), id_(id), dir_(dir) {
  CHECK(handler);
  CHECK_GE(id, 0);
  CHECK_LT(id, kGpioIdMax);
  CHECK_GE(dir, 0);
  CHECK_LT(dir, kGpioDirMax);
}

StandardGpioHandler::GpioDirResetter::~GpioDirResetter() {
  if (do_reset_ && !handler_->SetGpioDirection(id_, dir_)) {
    LOG(WARNING) << "failed to reset direction of " << gpio_defs_[id_].name
                 << " to " << gpio_dirs_[dir_];
  }
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

void StandardGpioHandler::ResetTestModeSignalingFlags() {
  is_first_check_ = true;
  is_handshake_completed_ = false;
  is_test_mode_ = false;
}

bool StandardGpioHandler::DiscoverGpios() {
  if (is_discovery_attempted_)
    return is_discovery_successful_;

  is_discovery_attempted_ = true;

  // Obtain libudev instance and attach to a dedicated closer.
  struct udev* udev;
  if (!(udev = udev_iface_->New())) {
    LOG(ERROR) << "failed to obtain libudev instance, aborting GPIO discovery";
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

  is_discovery_successful_ = true;
  return true;
}

bool StandardGpioHandler::GetGpioDevName(StandardGpioHandler::GpioId id,
                                         string* dev_path_p) {
  CHECK(id >= 0 && id < kGpioIdMax && dev_path_p);

  *dev_path_p = gpios_[id].dev_path;
  return true;
}

bool StandardGpioHandler::OpenGpioFd(StandardGpioHandler::GpioId id,
                                     const char* dev_name,
                                     bool is_write) {
  CHECK(id >= 0 && id < kGpioIdMax && dev_name);
  string file_name = StringPrintf("%s/%s", gpios_[id].dev_path.c_str(),
                                  dev_name);
  if (!fd_->Open(file_name.c_str(), (is_write ? O_WRONLY : O_RDONLY))) {
    if (fd_->IsSettingErrno()) {
      PLOG(ERROR) << "failed to open " << file_name
                  << " (" << gpio_defs_[id].name << ") for "
                  << (is_write ? "writing" : "reading");
    } else {
      LOG(ERROR) << "failed to open " << file_name
                 << " (" << gpio_defs_[id].name << ") for "
                 << (is_write ? "writing" : "reading");
    }
    return false;
  }
  return true;
}

bool StandardGpioHandler::SetGpio(StandardGpioHandler::GpioId id,
                                  const char* dev_name, const char* entries[],
                                  const int num_entries, int index) {
  CHECK_GE(id, 0);
  CHECK_LT(id, kGpioIdMax);
  CHECK(dev_name);
  CHECK(entries);
  CHECK_GT(num_entries, 0);
  CHECK_GE(index, 0);
  CHECK_LT(index, num_entries);

  // Open device for writing.
  if (!OpenGpioFd(id, dev_name, true))
    return false;
  ScopedFileDescriptorCloser dev_fd_closer(fd_);

  // Write a string corresponding to the requested output index to the GPIO
  // device, appending a newline.
  string output_str = entries[index];
  output_str += '\n';
  ssize_t write_len = fd_->Write(output_str.c_str(), output_str.length());
  if (write_len != static_cast<ssize_t>(output_str.length())) {
    if (write_len < 0) {
      const string err_str = "failed to write to GPIO";
      if (fd_->IsSettingErrno()) {
        PLOG(ERROR) << err_str;
      } else {
        LOG(ERROR) << err_str;
      }
    } else {
      LOG(ERROR) << "wrong number of bytes written (" << write_len
                 << " instead of " << output_str.length() << ")";
    }
    return false;
  }

  // Close the device explicitly, returning the close result.
  return fd_->Close();
}

bool StandardGpioHandler::GetGpio(StandardGpioHandler::GpioId id,
                                  const char* dev_name, const char* entries[],
                                  const int num_entries, int* index_p) {
  CHECK_GE(id, 0);
  CHECK_LT(id, kGpioIdMax);
  CHECK(dev_name);
  CHECK(entries);
  CHECK_GT(num_entries, 0);
  CHECK(index_p);

  // Open device for reading.
  if (!OpenGpioFd(id, dev_name, false))
    return false;
  ScopedFileDescriptorCloser dev_fd_closer(fd_);

  // Read the GPIO device. We attempt to read more than the max number of
  // characters expected followed by a newline, to ensure that we've indeed read
  // all the data available on the device.
  size_t max_entry_len = 0;
  for (int i = 0; i < num_entries; i++) {
    size_t entry_len = strlen(entries[i]);
    if (entry_len > max_entry_len)
      max_entry_len = entry_len;
  }
  max_entry_len++;  // account for trailing newline
  size_t buf_len = max_entry_len + 1;  // room for excess char / null terminator
  char buf[buf_len];
  memset(buf, 0, buf_len);
  ssize_t read_len = fd_->Read(buf, buf_len);
  if (read_len < 0 || read_len > static_cast<ssize_t>(max_entry_len)) {
    if (read_len < 0) {
      const string err_str = "failed to read GPIO";
      if (fd_->IsSettingErrno()) {
        PLOG(ERROR) << err_str;
      } else {
        LOG(ERROR) << err_str;
      }
    } else {
      LOG(ERROR) << "read too many bytes (" << read_len << ")";
    }
    return false;
  }

  // Remove trailing newline.
  read_len--;
  if (buf[read_len] != '\n') {
    LOG(ERROR) << "read value missing trailing newline";
    return false;
  }
  buf[read_len] = '\0';

  // Identify and write GPIO status.
  for (int i = 0; i < num_entries; i++)
    if (!strcmp(entries[i], buf)) {
      *index_p = i;
      // Close the device explicitly, returning the close result.
      return fd_->Close();
    }

  // Oops, unidentified reading...
  LOG(ERROR) << "read unexpected value from GPIO (`" << buf << "')";
  return false;
}

bool StandardGpioHandler::SetGpioDirection(StandardGpioHandler::GpioId id,
                                           StandardGpioHandler::GpioDir dir) {
  return SetGpio(id, "direction", gpio_dirs_, kGpioDirMax, dir);
}

bool StandardGpioHandler::GetGpioDirection(
    StandardGpioHandler::GpioId id,
    StandardGpioHandler::GpioDir* direction_p) {
  return GetGpio(id, "direction", gpio_dirs_, kGpioDirMax,
                 reinterpret_cast<int*>(direction_p));
}

bool StandardGpioHandler::SetGpioValue(StandardGpioHandler::GpioId id,
                                       StandardGpioHandler::GpioVal value,
                                       bool is_check_direction) {
  // If so instructed, ensure that the GPIO is indeed in the output direction
  // before attempting to write to it.
  if (is_check_direction) {
    GpioDir dir;
    if (!(GetGpioDirection(id, &dir) && dir == kGpioDirOut)) {
      LOG(ERROR) << "couldn't verify that GPIO is in the output direction "
                    "prior to reading from it";
      return false;
    }
  }

  return SetGpio(id, "value", gpio_vals_, kGpioValMax, value);
}

bool StandardGpioHandler::GetGpioValue(StandardGpioHandler::GpioId id,
                                       StandardGpioHandler::GpioVal* value_p,
                                       bool is_check_direction) {
  // If so instructed, ensure that the GPIO is indeed in the input direction
  // before attempting to read from it.
  if (is_check_direction) {
    GpioDir dir;
    if (!(GetGpioDirection(id, &dir) && dir == kGpioDirIn)) {
      LOG(ERROR) << "couldn't verify that GPIO is in the input direction "
                    "prior to reading from it";
      return false;
    }
  }

  return GetGpio(id, "value", gpio_vals_, kGpioValMax,
                 reinterpret_cast<int*>(value_p));
}

bool StandardGpioHandler::DoTestModeSignalingProtocol() {
  // The test mode signaling protocol is designed to provide a robust indication
  // that a Chrome OS device is physically connected to a servo board in a lab
  // setting. It is making very few assumptions about the soundness of the
  // hardware, firmware and kernel driver implementation of the GPIO mechanism.
  // In general, it is performing a three-way handshake between servo and the
  // Chrome OS client, based on changes in the GPIO value readings. The
  // client-side implementation does the following:
  //
  //  1. Check for an initial signal (0) on the input GPIO (dut_flaga).
  //
  //  2. Flip the signal (1 -> 0) on the output GPIO (dut_flagb).
  //
  //  3. Check for a flipped signal (1) on the input GPIO.
  //
  // TODO(garnold) the current implementation is based on sysfs access to GPIOs.
  // We will likely change this to using a specialized in-kernel driver
  // implementation, which would give us better performance and security
  // guarantees.

  LOG(INFO) << "attempting GPIO handshake";

  const char* dutflaga_name = gpio_defs_[kGpioIdDutflaga].name;
  const char* dutflagb_name = gpio_defs_[kGpioIdDutflagb].name;

  // Flip GPIO direction, set it to "in".
  // TODO(garnold) changing the GPIO direction back and forth is necessary for
  // overcoming a firmware/kernel issue which causes the device to be in the
  // "out" state whereas the kernel thinks it is in the "in" state.  This should
  // be abandoned once the firmware and/or kernel driver have been fixed.
  // Details here: http://code.google.com/p/chromium-os/issues/detail?id=27680
  if (!(SetGpioDirection(kGpioIdDutflaga, kGpioDirOut) &&
        SetGpioDirection(kGpioIdDutflaga, kGpioDirIn))) {
    LOG(ERROR) << "failed to flip direction of input GPIO " << dutflaga_name;
    return false;
  }

  // Peek input GPIO state.
  GpioVal dutflaga_gpio_value;
  if (!GetGpioValue(kGpioIdDutflaga, &dutflaga_gpio_value, true)) {
    LOG(ERROR) << "failed to read input GPIO " << dutflaga_name;
    return false;
  }

  // If initial handshake signal not received, abort.
  if (dutflaga_gpio_value != kGpioValDown) {
    LOG(INFO) << "input GPIO " << dutflaga_name
              << " unset, terminating handshake";
    is_handshake_completed_ = true;
    return true;
  }

  // Initialize output GPIO to a default state.
  // TODO(garnold) a similar workaround for possible driver/firmware glitches,
  // we insist on flipping the direction of the GPIO prior to assuming it is in
  // the "out" direction.
  GpioDirResetter dutflagb_dir_resetter(this, kGpioIdDutflagb, kGpioDirIn);
  if (!(SetGpioDirection(kGpioIdDutflagb, kGpioDirIn) &&
        dutflagb_dir_resetter.set_do_reset(
            SetGpioDirection(kGpioIdDutflagb, kGpioDirOut)) &&
        SetGpioValue(kGpioIdDutflagb, kGpioValUp, false))) {
    LOG(ERROR) << "failed to initialize output GPIO " << dutflagb_name;
    return false;
  }

  // Wait, giving the receiving end enough time to sense the fall.
  g_usleep(kServoOutputResponseWaitInSecs * G_USEC_PER_SEC);

  // Flip the output signal.
  if (!SetGpioValue(kGpioIdDutflagb, kGpioValDown, false)) {
    LOG(ERROR) << "failed to flip output GPIO " << dutflagb_name;
    return false;
  }

  // Look for flipped input GPIO value, up to a preset timeout.
  Time expires =
      Time::Now() + TimeDelta::FromSeconds(kServoInputResponseTimeoutInSecs);
  TimeDelta delay =
      TimeDelta::FromMicroseconds(1000000 / kServoInputNumChecksPerSec);
  bool is_first_response_check = true;
  bool is_error = false;
  while (Time::Now() < expires) {
    if (is_first_response_check)
      is_first_response_check = false;
    else
      g_usleep(delay.InMicroseconds());

    // Read input GPIO.
    if (!GetGpioValue(kGpioIdDutflaga, &dutflaga_gpio_value, true)) {
      LOG(ERROR) << "failed to read input GPIO " << dutflaga_name;
      is_error = true;
      break;
    }

    // If dutflaga is now up (flipped), we got our signal!
    if (dutflaga_gpio_value == kGpioValUp) {
      is_test_mode_ = true;
      break;
    }
  }

  if (!is_error) {
    if (is_test_mode_) {
      is_handshake_completed_ = true;
      LOG(INFO) << "GPIO handshake completed, test mode signaled";
    } else {
      LOG(INFO) << "timed out waiting for input GPIO " << dutflaga_name
                << " to flip, terminating handshake";
    }
  }

  return is_handshake_completed_;
}


bool NoopGpioHandler::IsTestModeSignaled() {
  LOG(INFO) << "GPIOs not engaged, defaulting to "
            << (is_test_mode_ ? "test" : "normal") << " mode";
  return is_test_mode_;
}

}  // namespace chromeos_update_engine
