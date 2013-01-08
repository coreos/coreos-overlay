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
  // This constructor accepts a udev interface |udev_iface| and a reusable file
  // descriptor |fd|. The value of |is_defer_discovery| determines whether GPIO
  // discovery should be attempted right away (false) or done lazily, when
  // necessitated by other calls (true). If |is_cache_test_mode| is true,
  // checking for test mode signal is done only once and further queries return
  // the cached result.
  StandardGpioHandler(UdevInterface* udev_iface, FileDescriptor* fd,
                      bool is_defer_discovery, bool is_cache_test_mode);

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

  // The number of seconds we wait before flipping the output signal (aka,
  // producing the "challenge" signal). Assuming a 1 second sampling frequency
  // on the servo side, a two second wait should be enough.
  static const int kServoOutputResponseWaitInSecs = 2;

  // The total number of seconds we wait for a servo response from the point we
  // flip the output signal. Assuming a 1 second sampling frequency on the servo
  // side, a two second wait should suffice. We add one more second for grace
  // (servod / hardware processing delays, etc).
  static const int kServoInputResponseTimeoutInSecs = 3;

  // The number of times per second we check for a servo response. Five seems
  // like a reasonable value.
  static const int kServoInputNumChecksPerSec = 5;

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

  // Helper class for resetting a GPIO direction.
  class GpioDirResetter {
   public:
    GpioDirResetter(StandardGpioHandler* handler, GpioId id, GpioDir dir);
    ~GpioDirResetter();

    bool do_reset() const {
      return do_reset_;
    }
    bool set_do_reset(bool do_reset) {
      return (do_reset_ = do_reset);
    }

   private:
    // Determines whether or not the GPIO direction should be reset to the
    // initial value.
    bool do_reset_;

    // The GPIO handler to use for changing the GPIO direction.
    StandardGpioHandler* handler_;

    // The GPIO identifier and initial direction.
    GpioId id_;
    GpioDir dir_;
  };

  // An initialization helper performing udev enumeration. |enum_helper|
  // implements an enumeration initialization and processing methods. Returns
  // true upon success, false otherwise.
  bool InitUdevEnum(struct udev* udev, UdevEnumHelper* enum_helper);

  // Resets the object's flags which determine the status of test mode
  // signaling.
  void ResetTestModeSignalingFlags();

  // Attempt GPIO discovery, at most once. Returns true if discovery process was
  // successfully completed or already attempted, false otherwise.
  bool DiscoverGpios();

  // Assigns a copy of the device name of GPIO |id| to |dev_path_p|. Assumes
  // initialization. Returns true upon success, false otherwise.
  bool GetGpioDevName(GpioId id, std::string* dev_path_p);

  // Open a sysfs file device |dev_name| of GPIO |id|, for either reading or
  // writing depending on |is_write|.  Uses the internal file descriptor for
  // this purpose, which can be reused as long as it is closed between
  // successive opens. Returns true upon success, false otherwise (optionally,
  // with errno set accordingly).
  bool OpenGpioFd(GpioId id, const char* dev_name, bool is_write);

  // Writes a value to device |dev_name| of GPIO |id|. The index |output| is
  // used to index the corresponding string to be written from the list
  // |entries| of length |num_entries|.  Returns true upon success, false
  // otherwise.
  bool SetGpio(GpioId id, const char* dev_name, const char* entries[],
               const int num_entries, int output);

  // Reads a value from device |dev_name| of GPIO |id|. The list |entries| of
  // length |num_entries| is used to convert the read string into an index,
  // which is written to |input_p|. The call will fail if the value being read
  // is not listed in |entries|. Returns true upon success, false otherwise.
  bool GetGpio(GpioId id, const char* dev_name, const char* entries[],
               const int num_entries, int* input_p);

  // Sets GPIO |id| to to operate in a given |direction|. Assumes
  // initialization. Returns true on success, false otherwise.
  bool SetGpioDirection(GpioId id, GpioDir direction);

  // Assigns the current direction of GPIO |id| into |direction_p|. Assumes
  // initialization. Returns true on success, false otherwise.
  bool GetGpioDirection(GpioId id, GpioDir* direction_p);

  // Sets the value of GPIO |id| to |value|. Assumues initialization. The GPIO
  // direction should be set to 'out' prior to this call. If
  // |is_check_direction| is true, it'll ensure that the direction is indeed
  // 'out' prior to attempting the write. Returns true on success, false
  // otherwise.
  bool SetGpioValue(GpioId id, GpioVal value, bool is_check_direction);

  // Reads the value of a GPIO |id| and stores it in |value_p|. Assumes
  // initialization.  The GPIO direction should be set to 'in' prior to this
  // call. If |is_check_direction| is true, it'll ensure that the direction is
  // indeed 'in' prior to attempting the read. Returns true upon success, false
  // otherwise.
  bool GetGpioValue(GpioId id, GpioVal *value_p, bool is_check_direction);

  // Invokes the actual GPIO handshake protocol to determine whether test mode
  // was signaled. Returns true iff the handshake has terminated gracefully
  // without encountering any errors; note that a true value does *not* mean
  // that a test mode signal has been detected.  The spec for this protocol:
  // https://docs.google.com/a/google.com/document/d/1DB-35ptck1wT1TYrgS5AC5Y3ALfHok-iPA7kLBw2XCI/edit
  bool DoTestModeSignalingProtocol();

  // Dynamic counter for the number of instances this class has. Used to enforce
  // that no more than one instance created. Thread-unsafe.
  static unsigned num_instances_;

  // GPIO control structures.
  Gpio gpios_[kGpioIdMax];

  // Udev interface.
  UdevInterface* const udev_iface_;

  // A file abstraction for handling GPIO devices.
  FileDescriptor* const fd_;

  // Determines whether test mode signal should be checked at most once and
  // cached, or reestablished on each query.
  const bool is_cache_test_mode_;

  // Indicates whether GPIO discovery was performed.
  bool is_discovery_attempted_;

  // Persistent state of the test mode check.
  bool is_first_check_;
  bool is_handshake_completed_;
  bool is_test_mode_;

  DISALLOW_COPY_AND_ASSIGN(StandardGpioHandler);
};


// A "no-op" GPIO handler, initialized to return either test or normal mode
// signal. This is useful for disabling the GPIO functionality in production
// code.
class NoopGpioHandler : public GpioHandler {
 public:
  // This constructor accepts a single argument, which is the value to be
  // returned by repeated calls to IsTestModeSignaled().
  NoopGpioHandler(bool is_test_mode) : is_test_mode_(is_test_mode) {}

  // Returns the constant Boolean value handed to the constructor.
  virtual bool IsTestModeSignaled();

 private:
  // Stores the constant value to return on subsequent test mode checks.
  bool is_test_mode_;

  DISALLOW_COPY_AND_ASSIGN(NoopGpioHandler);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_GPIO_HANDLER_H__
