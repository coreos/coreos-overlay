// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_GPIO_MOCK_FILE_DESCRIPTOR_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_GPIO_MOCK_FILE_DESCRIPTOR_H__

#include <base/rand_util.h>
#include <base/time.h>

#include "update_engine/file_descriptor.h"
#include "update_engine/gpio_handler_unittest.h"

// A set of mock file descriptors used for unit-testing. All classes here
// inherit the FileDescriptor interface.

namespace chromeos_update_engine {

// An abstract classs implementing a common mock infrastructure for GPIO
// reading/writing. This includes all the inherited interface methods, and basic
// logic to manage the opening, reading, writing and closing of GPIO files. It
// is up to concrete implementations to manage their internal state machine and
// update the values to be read by clients. In most cases, this amounts to
// adding internal state and overloading the UpdateState() method to change to
// various GPIO "registers" accordingly.
class GpioMockFileDescriptor : public FileDescriptor {
 public:
  GpioMockFileDescriptor();

  // Interface methods.
  virtual bool Open(const char* path, int flags, mode_t mode);
  virtual bool Open(const char* path, int flags);
  virtual ssize_t Read(void* buf, size_t count);
  virtual ssize_t Write(const void* buf, size_t count);
  virtual bool Close();
  virtual void Reset();
  virtual bool IsSettingErrno();
  virtual bool IsOpen() {
    return (gpio_id_ < kMockGpioIdMax && gpio_subdev_ < kMockGpioSubdevMax);
  }


  // Returns true iff all file resources were freed; otherwise, will fail the
  // current test.
  virtual bool ExpectAllResourcesDeallocated();

  // Returns true iff all GPIOs have been restored to their default state;
  // otherwise, will fail the current test.
  virtual bool ExpectAllGpiosRestoredToDefault();

  // Returns true iff the number of open attempts equals the argument;
  // otherwise, will fail the current test.
  virtual bool ExpectNumOpenAttempted(unsigned count);

 protected:
  // A pair of write value and time at which it was written.
  struct MockGpioWriteEvent {
    MockGpioVal val;
    base::Time time;
  };

  // Sets the last written value and timestamp of GPIO |gpio_id|.
  inline MockGpioVal SetGpioLastWrite(MockGpioId gpio_id, MockGpioVal val,
                                      base::Time time) {
    gpio_last_writes_[gpio_id].time = time;
    return (gpio_last_writes_[gpio_id].val = val);
  }

  inline MockGpioVal SetGpioLastWrite(MockGpioId gpio_id, MockGpioVal val) {
    return SetGpioLastWrite(gpio_id, val, base::Time::Now());
  }


  // The current direction of each GPIO device. These are generally handled by
  // Write(), but can be modified by concrete implementations of this class to
  // simulate race conditions on GPIO devices.
  MockGpioDir gpio_dirs_[kMockGpioIdMax];

  // The current values to be read by the DUT. These can be modified by concrete
  // implementations of this class, to reflect current GPIO values.
  MockGpioVal gpio_read_vals_[kMockGpioIdMax];

  // The last values and time they were written by the DUT to each GPIO device.
  // These are generally handled by Write(), but can be modified by concrete
  // implementations of this class to simulate race conditions on GPIO devices.
  MockGpioWriteEvent gpio_last_writes_[kMockGpioIdMax];

  // Override strings for GPIO value / direction readings. Initialized to null
  // pointers by default, which means the default values will not be overridden.
  const char* override_read_gpio_val_strings_[kMockGpioValMax];
  const char* override_read_gpio_dir_strings_[kMockGpioDirMax];

 private:
  // GPIO subdevice identifiers.
  enum MockGpioSubdev {
    kMockGpioSubdevValue,
    kMockGpioSubdevDirection,
    kMockGpioSubdevMax  // marker, do not remove!
  };

  // Device name prefixes of the different GPIOs.
  static const char* gpio_devname_prefixes_[kMockGpioIdMax];

  // Strings to be written as GPIO values, corresponding to the abstract GPIO
  // value.
  static const char* gpio_val_strings_[kMockGpioValMax];

  // Strings to be written as GPIO directions, corresponding to the abstract
  // GPIO direction.
  static const char* gpio_dir_strings_[kMockGpioDirMax];


  // Compare a string |buf| of length |count| that is written to a GPIO device
  // with an array of strings |strs| of length |num_strs|. Returns the index of
  // the string entry that is the same as the written string, or |num_strs| if
  // none was found. Requires that the the last character in |buf| is a newline.
  size_t DecodeGpioString(const char* buf, size_t count, const char** strs,
                          size_t num_strs) const;

  // Decode a written GPIO value.
  inline MockGpioVal DecodeGpioVal(const char* buf, size_t count) const {
    return static_cast<MockGpioVal>(
        DecodeGpioString(buf, count, gpio_val_strings_, kMockGpioValMax));
  }

  // Decodes a written GPIO direction.
  inline MockGpioDir DecodeGpioDir(const char* buf, size_t count) const {
    return static_cast<MockGpioDir>(
        DecodeGpioString(buf, count, gpio_dir_strings_, kMockGpioDirMax));
  }

  // Simulate the Servo state transition, based on the last recorded state, the
  // time it was recorded, and the current GPIO values. This is a pure virtual
  // function that must be implemented by concrete subclasses.
  virtual void UpdateState() = 0;

  // The identifier of the currently accessed GPIO device.
  MockGpioId gpio_id_;

  // The identifier of the currently accessed GPIO sub-device.
  MockGpioSubdev gpio_subdev_;

  // Counter for the number of files that were opened with this interface.
  unsigned num_open_attempted_;
};


// A mock file descriptor that implements the GPIO test signaling protocol. In
// doing so, it simulates the asynchronous behavior of a properly implemented
// Servo test controller.
class TestModeGpioMockFileDescriptor : public GpioMockFileDescriptor {
 public:
  TestModeGpioMockFileDescriptor(base::TimeDelta servo_poll_interval);
  virtual ~TestModeGpioMockFileDescriptor() {};

 protected:
  // The state of the Servo-side GPIO signaling protocol. These do not include
  // sub-state changes on the DUT side, which can be approximated by tracking
  // read operations but otherwise cannot be observed by an ordinary Servo.
  enum ServoState {
    kServoStateInit,
    kServoStateTriggerSent,
    kServoStateChallengeUpReceived,
    kServoStateChallengeDownReceived,
    kServoStateMax  // marker, do not remove!
  };

  // Simulate the Servo state transition, based on the last recorded state, the
  // time it was recorded, and the current GPIO values.
  virtual void UpdateState();

  // The last recorded state in the GPIO protocol.
  ServoState last_state_;

 private:
  // Return a uniformly distributed random time delta within the Servo poll
  // interval.
  inline base::TimeDelta RandomServoPollFuzz() {
    return base::TimeDelta::FromMicroseconds(
        base::RandInt(0, servo_poll_interval_.InMicroseconds()));
  }

  // The Servo poll interval.
  base::TimeDelta servo_poll_interval_;

  // The current Servo poll fuzz, used for deciding when signals are (simulated
  // to be) sensed within the poll interval. Must be between zero and
  // servo_poll_interval_.
  base::TimeDelta curr_servo_poll_fuzz_;
};


// A mock file descriptor that implements GPIO feedback when not conneced to a
// Servo, on boards that include a pull-up resistor wiring for GPIOs. This is
// the typical mode of operations for Chromebooks out in the field, and we need
// to make sure that the client is not made to believe that it is in test mode.
class NormalModeGpioMockFileDescriptor : public GpioMockFileDescriptor {
 private:
  // This is a no-op, as there's no Servo connected.
  virtual void UpdateState() {};
};

// A mock file descriptor that implements GPIOs that are not pulled-up by
// default, and whose idle reading might be zero. We've seen this problem on
// Lumpy/Stumpy and need to make sure that the protocol doesn't allow these
// boards to go into test mode without actually being told so by a Servo.
class NonPulledUpNormalModeGpioMockFileDescriptor
    : public GpioMockFileDescriptor {
 private:
  // Set the default value of dut_flaga to "down".
  virtual void UpdateState() {
    gpio_read_vals_[kMockGpioIdDutflaga] = kMockGpioValDown;
  }
};

// A mock file descriptor that implements a bogus GPIO feedback. This includes
// flipping GPIO directions, invalid value readings, and I/O errors on various
// file operations. All of these instances must be ruled out by the protocol,
// resulting in normal mode operation.
class ErrorNormalModeGpioMockFileDescriptor :
    public TestModeGpioMockFileDescriptor {
 public:
  enum GpioError {
    kGpioErrorFlipInputDir,
    kGpioErrorReadInvalidVal,
    kGpioErrorReadInvalidDir,
    kGpioErrorFailFileOpen,
    kGpioErrorFailFileRead,
    kGpioErrorFailFileWrite,
    kGpioErrorFailFileClose,
  };

  ErrorNormalModeGpioMockFileDescriptor(base::TimeDelta servo_poll_interval,
                                        GpioError error);
  virtual ~ErrorNormalModeGpioMockFileDescriptor() {};

  // Wrapper methods for the respectively inherited ones, which can fail the
  // call as part of a test.
  virtual bool Open(const char* path, int flags, mode_t mode);
  virtual ssize_t Read(void* buf, size_t count);
  virtual ssize_t Write(const void* buf, size_t count);
  virtual bool Close();

  // Wrapper which restores all state we might have tampered with.
  virtual bool ExpectAllGpiosRestoredToDefault();

 private:
  // Wraps the ordinary test mode servo simulation with an error injecting
  // behavior, which corresponds to the requested type of error.
  virtual void UpdateState();

  // The GPIO error to be injected into the protocol.
  GpioError error_;

  // A flag denoting whether the direction of dut_flaga was already maliciously
  // flipped.
  bool is_dutflaga_dir_flipped_;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_GPIO_MOCK_FILE_DESCRIPTOR_H__
