// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/gpio_mock_file_descriptor.h"

#include <base/stringprintf.h>
#include <gtest/gtest.h>

#include "update_engine/utils.h"

using base::Time;
using base::TimeDelta;
using std::string;

namespace chromeos_update_engine {

namespace {
// Typesets a time object into a string; omits the date.
string TimeToString(Time time) {
  Time::Exploded exploded_time;
  time.LocalExplode(&exploded_time);
  return StringPrintf("%d:%02d:%02d.%03d",
                      exploded_time.hour,
                      exploded_time.minute,
                      exploded_time.second,
                      exploded_time.millisecond);
}
}  // namespace

//
// GpioMockFileDescriptor
//
const char* GpioMockFileDescriptor::gpio_devname_prefixes_[kMockGpioIdMax] = {
  MOCK_SYSFS_PREFIX "/gpio" MOCK_DUTFLAGA_GPIO_ID,
  MOCK_SYSFS_PREFIX "/gpio" MOCK_DUTFLAGB_GPIO_ID,
};

const char* GpioMockFileDescriptor::gpio_val_strings_[kMockGpioValMax] = {
  "1",  // kMockGpioValUp
  "0",  // kMockGpioValDown
};

const char* GpioMockFileDescriptor::gpio_dir_strings_[kMockGpioDirMax] = {
  "in",   // kMockGpioDirIn
  "out",  // kMockGpioDirOut
};


GpioMockFileDescriptor::GpioMockFileDescriptor()
    : gpio_id_(kMockGpioIdMax),
      gpio_subdev_(kMockGpioSubdevMax) {
  // All GPIOs are initially in the input direction, their read value is "up",
  // and they assume an initial write value of "up" with current (init) time.
  Time init_time = Time::Now();
  for (size_t i = 0; i < kMockGpioIdMax; i++) {
    gpio_dirs_[i] = kMockGpioDirIn;
    gpio_read_vals_[i] = kMockGpioValUp;
    SetGpioLastWrite(static_cast<MockGpioId>(i), kMockGpioValUp, init_time);
  }

  // Nullify the instance-specific override strings.
  for (size_t i = 0; i < kMockGpioValMax; i++)
    override_read_gpio_val_strings_[i] = NULL;
  for (size_t i = 0; i < kMockGpioDirMax; i++)
    override_read_gpio_dir_strings_[i] = NULL;
}

bool GpioMockFileDescriptor::Open(const char* path, int flags, mode_t mode) {
  EXPECT_EQ(gpio_id_, kMockGpioIdMax);
  if (gpio_id_ != kMockGpioIdMax)
    return false;

  // Determine identifier of opened GPIO device.
  size_t devname_prefix_len = 0;
  int id;
  for (id = 0; id < kMockGpioIdMax; id++) {
    devname_prefix_len = strlen(gpio_devname_prefixes_[id]);
    if (!strncmp(path, gpio_devname_prefixes_[id], devname_prefix_len))
      break;
  }
  EXPECT_LT(id, kMockGpioIdMax);
  if (id == kMockGpioIdMax)
    return false;

  // Determine specific sub-device.
  path += devname_prefix_len;
  EXPECT_EQ(path[0], '/');
  if (path[0] != '/')
    return false;
  path++;
  if (!strcmp(path, "value"))
    gpio_subdev_ = kMockGpioSubdevValue;
  else if (!strcmp(path, "direction"))
    gpio_subdev_ = kMockGpioSubdevDirection;
  else {
    ADD_FAILURE();
    return false;
  }

  gpio_id_ = static_cast<MockGpioId>(id);
  LOG(INFO) << "opened mock gpio "
            << (id == kMockGpioIdDutflaga ? "dut_flaga" :
                id == kMockGpioIdDutflagb ? "dut_flagb" :
                "<unknown>")
            << "/"
            << (gpio_subdev_ == kMockGpioSubdevValue ? "value" :
                gpio_subdev_ == kMockGpioSubdevDirection ? "direction" :
                "<unknown>");
  return true;
}

bool GpioMockFileDescriptor::Open(const char* path, int flags) {
  return Open(path, flags, 0);
}

ssize_t GpioMockFileDescriptor::Read(void* buf, size_t count) {
  EXPECT_TRUE(IsOpen());
  if (!IsOpen())
    return -1;

  LOG(INFO) << "reading from gpio";

  // Attempt a state update prior to responding to the read.
  UpdateState();

  switch (gpio_subdev_) {
    case kMockGpioSubdevValue: {  // reading the GPIO value
      // Read values vary depending on the GPIO's direction: an input GPIO will
      // return the value that was written by the remote end; an output GPIO,
      // however, will return the value last written to its output register...
      MockGpioVal gpio_read_val = kMockGpioValMax;
      switch (gpio_dirs_[gpio_id_]) {
        case kMockGpioDirIn:
          gpio_read_val = gpio_read_vals_[gpio_id_];
          break;
        case kMockGpioDirOut:
          gpio_read_val = gpio_last_writes_[gpio_id_].val;
          break;
        default:
          CHECK(false);  // shouldn't get here
      }

      // Write the value to the client's buffer.
      return snprintf(reinterpret_cast<char*>(buf), count, "%s\n",
                      (override_read_gpio_val_strings_[gpio_read_val] ?
                       override_read_gpio_val_strings_[gpio_read_val] :
                       gpio_val_strings_[gpio_read_val]));
    }

    case kMockGpioSubdevDirection: {  // reading the GPIO direction
      // Write the current GPIO direction to the client's buffer.
      MockGpioDir gpio_dir = gpio_dirs_[gpio_id_];
      return snprintf(reinterpret_cast<char*>(buf), count, "%s\n",
                      (override_read_gpio_dir_strings_[gpio_dir] ?
                       override_read_gpio_dir_strings_[gpio_dir] :
                       gpio_dir_strings_[gpio_dir]));
    }

    default:
      ADD_FAILURE();  // shouldn't get here
      return -1;
  }
}

ssize_t GpioMockFileDescriptor::Write(const void* buf, size_t count) {
  EXPECT_TRUE(IsOpen());
  EXPECT_TRUE(buf);
  if (!(IsOpen() && buf))
    return -1;

  string str = StringPrintf("%-*s", static_cast<int>(count),
                            reinterpret_cast<const char*>(buf));
  size_t pos = 0;
  while ((pos = str.find('\n', pos)) != string::npos) {
    str.replace(pos, 1, "\\n");
    pos += 2;
  }
  LOG(INFO) << "writing to gpio: \"" << str << "\"";

  // Attempt a state update prior to performing the write operation.
  UpdateState();

  switch (gpio_subdev_) {
    case kMockGpioSubdevValue: {  // setting the GPIO value
      // Ensure the GPIO is in the "out" direction
      EXPECT_EQ(gpio_dirs_[gpio_id_], kMockGpioDirOut);
      if (gpio_dirs_[gpio_id_] != kMockGpioDirOut)
        return -1;

      // Decode the written value.
      MockGpioVal write_val = DecodeGpioVal(reinterpret_cast<const char*>(buf),
                                            count);
      EXPECT_LT(write_val, kMockGpioValMax);
      if (write_val == kMockGpioValMax)
        return -1;

      // Update the last tracked written value.
      SetGpioLastWrite(gpio_id_, write_val);
      break;
    }

    case kMockGpioSubdevDirection: {  // setting GPIO direction
      // Decipher the direction to be set.
      MockGpioDir write_dir = DecodeGpioDir(reinterpret_cast<const char*>(buf),
                                            count);
      EXPECT_LT(write_dir, kMockGpioDirMax);
      if (write_dir == kMockGpioDirMax)
        return -1;

      // Update the last write time for this GPIO if switching from "in" to
      // "out" and the written value is different from its read value; this is
      // due to the GPIO's DUT-side override, which may cause the Servo-side
      // reading to flip when switching it to "out".
      if (gpio_dirs_[gpio_id_] == kMockGpioDirIn &&
          write_dir == kMockGpioDirOut &&
          gpio_read_vals_[gpio_id_] != gpio_last_writes_[gpio_id_].val)
        gpio_last_writes_[gpio_id_].time = Time::Now();

      // Now update the GPIO direction.
      gpio_dirs_[gpio_id_] = write_dir;
      break;
    }

    default:
      ADD_FAILURE();  // shouldn't get here
      return -1;
  }

  return count;
}

bool GpioMockFileDescriptor::Close() {
  EXPECT_TRUE(IsOpen());
  if (!IsOpen())
    return false;

  Reset();
  return true;
}

void GpioMockFileDescriptor::Reset() {
  gpio_id_ = kMockGpioIdMax;
}

bool GpioMockFileDescriptor::IsSettingErrno() {
  // This mock doesn't test errno handling, so no.
  return false;
}

bool GpioMockFileDescriptor::ExpectAllResourcesDeallocated() {
  EXPECT_EQ(gpio_id_, kMockGpioIdMax);
  return (gpio_id_ == kMockGpioIdMax);
}

bool GpioMockFileDescriptor::ExpectAllGpiosRestoredToDefault() {
  // We just verify that direction is restored to "in" for all GPIOs.
  bool is_all_gpios_restored_to_default = true;
  for (size_t i = 0; i < kMockGpioIdMax; i++) {
    EXPECT_EQ(gpio_dirs_[i], kMockGpioDirIn)
        << "i=" << i << " gpio_dirs_[i]=" << gpio_dirs_[i];
    is_all_gpios_restored_to_default =
        is_all_gpios_restored_to_default && (gpio_dirs_[i] == kMockGpioDirIn);
  }
  return is_all_gpios_restored_to_default;
}

size_t GpioMockFileDescriptor::DecodeGpioString(const char* buf,
                                                      size_t count,
                                                      const char** strs,
                                                      size_t num_strs) const {
  CHECK(buf && strs && count);

  // Last character must be a newline.
  count--;
  if (buf[count] != '\n')
    return num_strs;

  // Scan for a precise match within the provided string array.
  size_t i;
  for (i = 0; i < num_strs; i++)
    if (count == strlen(strs[i]) &&
        !strncmp(buf, strs[i], count))
      break;
  return i;
}

//
// TestModeGpioMockFileDescriptor
//
TestModeGpioMockFileDescriptor::TestModeGpioMockFileDescriptor(
    TimeDelta servo_poll_interval)
    : last_state_(kServoStateInit),
      servo_poll_interval_(servo_poll_interval) {}

void TestModeGpioMockFileDescriptor::UpdateState() {
  // The following simulates the Servo state transition logic. Note that all of
  // these tests are (should be) conservative estimates of the actual,
  // asynchronous logic implemented by an actual Servo. Also, they should remain
  // so regardless of which operation (read, write) triggers the check. We
  // repeat the update cycle until no further state changes occur (which assumes
  // that there are no state transition cycles).
  Time curr_time = Time::Now();
  ServoState curr_state = last_state_;
  do {
    if (last_state_ != curr_state) {
      last_state_ = curr_state;
      curr_servo_poll_fuzz_ = RandomServoPollFuzz();  // fix a new poll fuzz
      LOG(INFO) << "state=" << last_state_ << ", new poll fuzz="
                << utils::FormatTimeDelta(curr_servo_poll_fuzz_);
    }

    switch (last_state_) {
      case kServoStateInit:
        // Unconditionally establish the trigger signal.
        LOG(INFO) << "unconditionally sending trigger signal over dut_flaga";
        gpio_read_vals_[kMockGpioIdDutflaga] = kMockGpioValDown;
        curr_state = kServoStateTriggerSent;
        break;

      case kServoStateTriggerSent:
        // If dut_flagb is in "out" mode, its last written value is "1", and
        // it's probable that Servo has polled it since, then advance the state.
        if (gpio_dirs_[kMockGpioIdDutflagb] == kMockGpioDirOut &&
            gpio_last_writes_[kMockGpioIdDutflagb].val == kMockGpioValUp &&
            (gpio_last_writes_[kMockGpioIdDutflagb].time +
             curr_servo_poll_fuzz_) < curr_time) {
          LOG(INFO) << "an up signal was written to dut_flagb on "
                    << TimeToString(gpio_last_writes_[kMockGpioIdDutflagb].time)
                    << " and polled at "
                    << TimeToString(
                           gpio_last_writes_[kMockGpioIdDutflagb].time +
                           curr_servo_poll_fuzz_)
                    << " (after "
                    << utils::FormatTimeDelta(curr_servo_poll_fuzz_)
                    << "); current time is " << TimeToString(curr_time);
          curr_state = kServoStateChallengeUpReceived;
        }
        break;

      case kServoStateChallengeUpReceived:
        // If dut_flagb is in "out" mode, its last written value is "0", and
        // it's probable that Servo has polled it since, then advance the state
        // and flip the value of dut_flaga.
        if (gpio_dirs_[kMockGpioIdDutflagb] == kMockGpioDirOut &&
            gpio_last_writes_[kMockGpioIdDutflagb].val == kMockGpioValDown &&
            (gpio_last_writes_[kMockGpioIdDutflagb].time +
             curr_servo_poll_fuzz_) < curr_time) {
          LOG(INFO) << "a down signal was written to dut_flagb on "
                    << TimeToString(gpio_last_writes_[kMockGpioIdDutflagb].time)
                    << " and polled at "
                    << TimeToString(
                           gpio_last_writes_[kMockGpioIdDutflagb].time +
                           curr_servo_poll_fuzz_)
                    << " (after "
                    << utils::FormatTimeDelta(curr_servo_poll_fuzz_)
                    << "); current time is " << TimeToString(curr_time);
          gpio_read_vals_[kMockGpioIdDutflaga] = kMockGpioValUp;
          curr_state = kServoStateChallengeDownReceived;
        }
        break;

      case kServoStateChallengeDownReceived:
        break;  // terminal state, nothing to do

      default:
        CHECK(false);  // shouldn't get here
    }
  } while (last_state_ != curr_state);
}

//
// ErrorNormalModeGpioMockFileDescriptor
//
ErrorNormalModeGpioMockFileDescriptor::ErrorNormalModeGpioMockFileDescriptor(
    TimeDelta servo_poll_interval,
    ErrorNormalModeGpioMockFileDescriptor::GpioError error)
    : TestModeGpioMockFileDescriptor(servo_poll_interval),
      error_(error),
      is_dutflaga_dir_flipped_(false) {}

bool ErrorNormalModeGpioMockFileDescriptor::Open(const char* path, int flags,
                                                 mode_t mode) {
  if (error_ == kGpioErrorFailFileOpen)
    return false;
  return TestModeGpioMockFileDescriptor::Open(path, flags, mode);
}

ssize_t ErrorNormalModeGpioMockFileDescriptor::Read(void* buf, size_t count) {
  if (error_ == kGpioErrorFailFileRead)
    return -1;
  return TestModeGpioMockFileDescriptor::Read(buf, count);
}

ssize_t ErrorNormalModeGpioMockFileDescriptor::Write(const void* buf,
                                                     size_t count) {
  if (error_ == kGpioErrorFailFileWrite)
    return -1;
  return TestModeGpioMockFileDescriptor::Write(buf, count);
}

bool ErrorNormalModeGpioMockFileDescriptor::Close() {
  // We actually need to perform the close operation anyway, to avoid
  // inconsistencies in the file descriptor's state.
  bool ret = TestModeGpioMockFileDescriptor::Close();
  return (error_ == kGpioErrorFailFileClose ? false : ret);
}

void ErrorNormalModeGpioMockFileDescriptor::UpdateState() {
  // Invoke the base class's update method.
  TestModeGpioMockFileDescriptor::UpdateState();

  // Sabotage the normal feedback that is to be expected from the GPIOs, in
  // various ways based on the requested type of error.
  switch (error_) {
    case kGpioErrorFlipInputDir:
      // Intervene by flipping the direction of dut_flaga right after the
      // challenge signal was sent to servo. Among other things, this could
      // simulate a benign race condition, or an intentional attempt to fool the
      // GPIO module to believe that it is talking to an (absent) servo.
      if (!is_dutflaga_dir_flipped_ &&
          last_state_ == kServoStateChallengeDownReceived) {
        is_dutflaga_dir_flipped_ = true;
        LOG(INFO) << "intervention: setting dut_flaga direction to out";
        gpio_dirs_[kMockGpioIdDutflaga] = kMockGpioDirOut;
      }
      break;

    case kGpioErrorReadInvalidVal:
      // Cause the GPIO device to return an invalid value reading.
      override_read_gpio_val_strings_[kMockGpioValUp] = "foo";
      override_read_gpio_val_strings_[kMockGpioValDown] = "bar";
      break;

    case kGpioErrorReadInvalidDir:
      // Cause the GPIO device to return an invlid direction reading.
      override_read_gpio_dir_strings_[kMockGpioDirIn] = "boo";
      override_read_gpio_dir_strings_[kMockGpioDirOut] = "far";

    case kGpioErrorFailFileOpen:
    case kGpioErrorFailFileRead:
    case kGpioErrorFailFileWrite:
    case kGpioErrorFailFileClose:
      break;

    default:
      CHECK(false);  // shouldn't get here
  }
}

bool ErrorNormalModeGpioMockFileDescriptor::ExpectAllGpiosRestoredToDefault() {
  if (is_dutflaga_dir_flipped_) {
    LOG(INFO) << "restoring dut_flaga direction back to in";
    gpio_dirs_[kMockGpioIdDutflaga] = kMockGpioDirIn;
  }

  return TestModeGpioMockFileDescriptor::ExpectAllGpiosRestoredToDefault();
}

}  // namespace chromeos_update_engine
