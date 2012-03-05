// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpio_handler.h"

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/eintr_wrapper.h>
#include <base/string_util.h>
#include <glib.h>

#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

const char* GpioHandler::dutflaga_dev_name_ = NULL;
const char* GpioHandler::dutflagb_dev_name_ = NULL;

namespace {
// Names of udev properties that are linked to the GPIO chip device and identify
// the two dutflag GPIOs on different boards.
const char kIdGpioDutflaga[] = "ID_GPIO_DUTFLAGA";
const char kIdGpioDutflagb[] = "ID_GPIO_DUTFLAGB";

// Scoped closer for udev and udev_enumerate objects.
// TODO(garnold) chromium-os:26934: it would be nice to generalize the different
// ScopedFooCloser implementations in update engine using a single template.
class ScopedUdevCloser {
 public:
  explicit ScopedUdevCloser(udev** udev_p) : udev_p_(udev_p) {}
  ~ScopedUdevCloser() {
    if (udev_p_ && *udev_p_) {
      udev_unref(*udev_p_);
      *udev_p_ = NULL;
    }
  }
 private:
  struct udev **udev_p_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUdevCloser);
};

class ScopedUdevEnumerateCloser {
 public:
  explicit ScopedUdevEnumerateCloser(udev_enumerate **udev_enum_p) :
    udev_enum_p_(udev_enum_p) {}
  ~ScopedUdevEnumerateCloser() {
    if (udev_enum_p_ && *udev_enum_p_) {
      udev_enumerate_unref(*udev_enum_p_);
      *udev_enum_p_ = NULL;
    }
  }
 private:
  struct udev_enumerate** udev_enum_p_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUdevEnumerateCloser);
};
}  // namespace {}

bool GpioHandler::IsGpioSignalingTest() {
  // Peek dut_flaga GPIO state.
  bool dutflaga_gpio_state;
  if (!GetDutflagGpioStatus(kDutflagaGpio, &dutflaga_gpio_state)) {
    LOG(WARNING) << "dutflaga GPIO reading failed, defaulting to non-test mode";
    return false;
  }

  LOG(INFO) << "dutflaga GPIO reading: "
            << (dutflaga_gpio_state ? "on (non-test mode)" : "off (test mode)");
  return !dutflaga_gpio_state;
}

bool GpioHandler::GetDutflagGpioDevName(struct udev* udev,
                                        const string& gpio_dutflag_str,
                                        const char** dutflag_dev_name_p) {
  CHECK(udev && dutflag_dev_name_p);

  struct udev_enumerate* udev_enum = NULL;
  int num_gpio_dutflags = 0;
  const string gpio_dutflag_pattern = "*" + gpio_dutflag_str;
  int ret;

  // Initialize udev enumerate context and closer.
  if (!(udev_enum = udev_enumerate_new(udev))) {
    LOG(ERROR) << "failed to obtain udev enumerate context";
    return false;
  }
  ScopedUdevEnumerateCloser udev_enum_closer(&udev_enum);

  // Populate filters for find an initialized GPIO chip.
  if ((ret = udev_enumerate_add_match_subsystem(udev_enum, "gpio")) ||
      (ret = udev_enumerate_add_match_sysname(udev_enum,
                                              gpio_dutflag_pattern.c_str()))) {
    LOG(ERROR) << "failed to initialize udev enumerate context (" << ret << ")";
    return false;
  }

  // Obtain list of matching devices.
  if ((ret = udev_enumerate_scan_devices(udev_enum))) {
    LOG(ERROR) << "udev enumerate context scan failed (error code "
               << ret << ")";
    return false;
  }

  // Iterate over matching devices, obtain GPIO dut_flaga identifier.
  struct udev_list_entry* list_entry;
  udev_list_entry_foreach(list_entry,
                          udev_enumerate_get_list_entry(udev_enum)) {
    // Make sure we're not enumerating more than one device.
    num_gpio_dutflags++;
    if (num_gpio_dutflags > 1) {
      LOG(WARNING) <<
        "enumerated multiple dutflag GPIOs, ignoring this one";
      continue;
    }

    // Obtain device name.
    const char* dev_name = udev_list_entry_get_name(list_entry);
    if (!dev_name) {
      LOG(WARNING) << "enumerated device has a null name string, skipping";
      continue;
    }

    // Obtain device object.
    struct udev_device* dev = udev_device_new_from_syspath(udev, dev_name);
    if (!dev) {
      LOG(WARNING) <<
        "obtained a null device object for enumerated device, skipping";
      continue;
    }

    // Obtain device syspath.
    const char* dev_syspath = udev_device_get_syspath(dev);
    if (dev_syspath) {
      LOG(INFO) << "obtained device syspath: " << dev_syspath;
      *dutflag_dev_name_p = strdup(dev_syspath);
    } else {
      LOG(WARNING) << "could not obtain device syspath";
    }

    udev_device_unref(dev);
  }

  return true;
}

bool GpioHandler::GetDutflagGpioDevNames(string* dutflaga_dev_name_p,
                                         string* dutflagb_dev_name_p) {
  if (!(dutflaga_dev_name_p || dutflagb_dev_name_p))
    return true;  // No output pointers, nothing to do.

  string gpio_dutflaga_str, gpio_dutflagb_str;

  if (!(dutflaga_dev_name_ && dutflagb_dev_name_)) {
    struct udev* udev = NULL;
    struct udev_enumerate* udev_enum = NULL;
    int num_gpio_chips = 0;
    const char* id_gpio_dutflaga = NULL;
    const char* id_gpio_dutflagb = NULL;
    int ret;

    LOG(INFO) << "begin discovery of dut_flaga/b devices";

    // Obtain libudev instance and closer.
    if (!(udev = udev_new())) {
      LOG(ERROR) << "failed to obtain libudev instance";
      return false;
    }
    ScopedUdevCloser udev_closer(&udev);

    // Initialize a udev enumerate object and closer with a bounded lifespan.
    {
      if (!(udev_enum = udev_enumerate_new(udev))) {
        LOG(ERROR) << "failed to obtain udev enumerate context";
        return false;
      }
      ScopedUdevEnumerateCloser udev_enum_closer(&udev_enum);

      // Populate filters for find an initialized GPIO chip.
      if ((ret = udev_enumerate_add_match_subsystem(udev_enum, "gpio")) ||
          (ret = udev_enumerate_add_match_sysname(udev_enum, "gpiochip*")) ||
          (ret = udev_enumerate_add_match_property(udev_enum,
                                                   kIdGpioDutflaga, "*")) ||
          (ret = udev_enumerate_add_match_property(udev_enum,
                                                   kIdGpioDutflagb, "*"))) {
        LOG(ERROR) << "failed to initialize udev enumerate context ("
                   << ret << ")";
        return false;
      }

      // Obtain list of matching devices.
      if ((ret = udev_enumerate_scan_devices(udev_enum))) {
        LOG(ERROR) << "udev enumerate context scan failed (" << ret << ")";
        return false;
      }

      // Iterate over matching devices, obtain GPIO dut_flaga identifier.
      struct udev_list_entry* list_entry;
      udev_list_entry_foreach(list_entry,
                              udev_enumerate_get_list_entry(udev_enum)) {
        // Make sure we're not enumerating more than one device.
        num_gpio_chips++;
        if (num_gpio_chips > 1) {
          LOG(WARNING) << "enumerated multiple GPIO chips, ignoring this one";
          continue;
        }

        // Obtain device name.
        const char* dev_name = udev_list_entry_get_name(list_entry);
        if (!dev_name) {
          LOG(WARNING) << "enumerated device has a null name string, skipping";
          continue;
        }

        // Obtain device object.
        struct udev_device* dev = udev_device_new_from_syspath(udev, dev_name);
        if (!dev) {
          LOG(WARNING) <<
            "obtained a null device object for enumerated device, skipping";
          continue;
        }

        // Obtain dut_flaga/b identifiers.
        id_gpio_dutflaga =
            udev_device_get_property_value(dev, kIdGpioDutflaga);
        id_gpio_dutflagb =
            udev_device_get_property_value(dev, kIdGpioDutflagb);
        if (id_gpio_dutflaga && id_gpio_dutflagb) {
          LOG(INFO) << "found dut_flaga/b identifiers: a=" << id_gpio_dutflaga
                    << " b=" << id_gpio_dutflagb;

          gpio_dutflaga_str = id_gpio_dutflaga;
          gpio_dutflagb_str = id_gpio_dutflagb;
        } else {
          LOG(ERROR) << "GPIO chip missing dut_flaga/b properties";
        }

        udev_device_unref(dev);
      }
    }

    // Obtain dut_flaga, reusing the same udev instance.
    if (!dutflaga_dev_name_ && !gpio_dutflaga_str.empty()) {
      LOG(INFO) << "discovering device for GPIO dut_flaga ";
      if (!GetDutflagGpioDevName(udev, gpio_dutflaga_str,
                                 &dutflaga_dev_name_)) {
        LOG(ERROR) << "discovery of dut_flaga GPIO device failed";
        return false;
      }
    }

    // Now obtain dut_flagb.
    if (!dutflagb_dev_name_ && !gpio_dutflagb_str.empty()) {
      LOG(INFO) << "discovering device for GPIO dut_flagb";
      if (!GetDutflagGpioDevName(udev, gpio_dutflagb_str,
                                 &dutflagb_dev_name_)) {
        LOG(ERROR) << "discovery of dut_flagb GPIO device failed";
        return false;
      }
    }

    LOG(INFO) << "end discovery of dut_flaga/b devices";
  }

  // Write cached GPIO dutflag(s) to output strings.
  if (dutflaga_dev_name_p && dutflaga_dev_name_)
    *dutflaga_dev_name_p = dutflaga_dev_name_;
  if (dutflagb_dev_name_p && dutflagb_dev_name_)
    *dutflagb_dev_name_p = dutflagb_dev_name_;

  return true;
}

bool GpioHandler::GetDutflagGpioStatus(GpioHandler::DutflagGpioId id,
                                       bool* status_p) {
  CHECK(status_p);

  // Obtain GPIO device file name.
  string dutflag_dev_name;
  switch (id) {
    case kDutflagaGpio:
      GetDutflagGpioDevNames(&dutflag_dev_name, NULL);
      break;
    case kDutflagbGpio:
      GetDutflagGpioDevNames(NULL, &dutflag_dev_name);
      break;
    default:
      LOG(FATAL) << "invalid dutflag GPIO id: " << id;
  }

  if (dutflag_dev_name.empty()) {
    LOG(WARNING) << "could not find dutflag GPIO device";
    return false;
  }

  // Open device for reading.
  string dutflaga_value_dev_name = dutflag_dev_name + "/value";
  int dutflaga_fd = HANDLE_EINTR(open(dutflaga_value_dev_name.c_str(), 0));
  if (dutflaga_fd < 0) {
    PLOG(ERROR) << "opening dutflaga GPIO device file failed";
    return false;
  }
  ScopedEintrSafeFdCloser dutflaga_fd_closer(&dutflaga_fd);

  // Read the dut_flaga GPIO signal. We attempt to read more than---but expect
  // to receive exactly---two characters: a '0' or '1', and a newline. This is
  // to ensure that the GPIO device returns a legible result.
  char buf[3];
  int ret = HANDLE_EINTR(read(dutflaga_fd, buf, 3));
  if (ret != 2) {
    if (ret < 0)
      PLOG(ERROR) << "reading dutflaga GPIO status failed";
    else
      LOG(ERROR) << "read more than one byte (" << ret << ")";
    return false;
  }

  // Identify and write GPIO status.
  char c = buf[0];
  if ((c == '0' || c == '1') && buf[1] == '\n') {
    *status_p = (c == '1');
  } else {
    buf[2] = '\0';
    LOG(ERROR) << "read unexpected value from dutflaga GPIO: " << buf;
    return false;
  }

  return true;
}

}  // namespace chromeos_update_engine
