// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/set_bootable_flag_action.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <string>
#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

void SetBootableFlagAction::PerformAction() {
  ScopedActionCompleter completer(processor_, this);

  if (!HasInputObject() || GetInputObject().empty()) {
    LOG(ERROR) << "SetBootableFlagAction: No input object.";
    return;
  }
  string device = GetInputObject();

  if (device.size() < 2) {
    LOG(ERROR) << "Device name too short: " << device;
    return;
  }

  // Make sure device is valid.
  const char last_char = device[device.size() - 1];
  if ((last_char < '1') || (last_char > '4')) {
    LOG(ERROR) << "Bad device:" << device;
    return;
  }

  // We were passed the partition_number'th partition; indexed from 1, not 0
  int partition_number = last_char - '0';

  const char second_to_last_char = device[device.size() - 2];
  if ((second_to_last_char >= '0') && (second_to_last_char <= '9')) {
    LOG(ERROR) << "Bad device:" << device;
    return;
  }

  // Strip trailing 1-4 off end of device
  device.resize(device.size() - 1);

  char mbr[512];  // MBR is the first 512 bytes of the device
  if (!ReadMbr(mbr, sizeof(mbr), device.c_str()))
    return;

  // Check MBR. Verify that last two byes are 0x55aa. This is the MBR signature.
  if ((mbr[510] != static_cast<char>(0x55)) ||
      (mbr[511] != static_cast<char>(0xaa))) {
    LOG(ERROR) << "Bad MBR signature";
    return;
  }

  // Mark partition passed in bootable and all other partitions non bootable.
  // There are 4 partition table entries, each 16 bytes, stored consecutively
  // beginning at byte 446. Within each entry, the first byte is the
  // bootable flag. It's set to 0x80 (bootable) or 0x00 (not bootable).
  for (int i = 0; i < 4; i++) {
    int offset = 446 + 16 * i;

    // partition_number is indexed from 1, not 0
    if ((i + 1) == partition_number)
      mbr[offset] = 0x80;
    else
      mbr[offset] = '\0';
  }

  // Write MBR back to disk
  bool success = WriteMbr(mbr, sizeof(mbr), device.c_str());
  if (success) {
    completer.set_success(true);
    if (HasOutputPipe()) {
      SetOutputObject(GetInputObject());
    }
  }
}

bool SetBootableFlagAction::ReadMbr(char* buffer,
                                    int buffer_len,
                                    const char* device) {
  int fd = open(device, O_RDONLY, 0);
  TEST_AND_RETURN_FALSE(fd >= 0);

  ssize_t r = read(fd, buffer, buffer_len);
  close(fd);
  TEST_AND_RETURN_FALSE(r == buffer_len);

  return true;
}

bool SetBootableFlagAction::WriteMbr(const char* buffer,
                                     int buffer_len,
                                     const char* device) {
  int fd = open(device, O_WRONLY, 0666);
  TEST_AND_RETURN_FALSE(fd >= 0);
  ScopedFdCloser fd_closer(&fd);

  ssize_t bytes_written = 0;
  while (bytes_written < buffer_len) {
    ssize_t r = write(fd, buffer + bytes_written, buffer_len - bytes_written);
    TEST_AND_RETURN_FALSE_ERRNO(r >= 0);
    bytes_written += r;
  }
  TEST_AND_RETURN_FALSE(bytes_written == buffer_len);

  return true;
}


}  // namespace chromeos_update_engine
