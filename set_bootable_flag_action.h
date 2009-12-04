// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_SET_BOOTABLE_FLAG_ACTION_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_SET_BOOTABLE_FLAG_ACTION_H__

#include <string>
#include "update_engine/action.h"

// This class takes in a device via the input pipe.  The device is the
// partition (e.g. /dev/sda1), not the full device (e.g. /dev/sda).
// It will make that device bootable by editing the partition table
// in the root device. Currently, this class doesn't support extended
// partitions.

namespace chromeos_update_engine {

class SetBootableFlagAction;

template<>
class ActionTraits<SetBootableFlagAction> {
 public:
  // Takes the device path as input.
  typedef std::string InputObjectType;
  // Passes the device path as output
  typedef std::string OutputObjectType;
};

class SetBootableFlagAction : public Action<SetBootableFlagAction> {
 public:
  SetBootableFlagAction() {}
  typedef ActionTraits<SetBootableFlagAction>::InputObjectType
      InputObjectType;
  typedef ActionTraits<SetBootableFlagAction>::OutputObjectType
      OutputObjectType;
  void PerformAction();

  // This is a synchronous action, and thus TerminateProcessing() should
  // never be called
  void TerminateProcessing() { CHECK(false); }

  // Debugging/logging
  static std::string StaticType() { return "SetBootableFlagAction"; }
  std::string Type() const { return StaticType(); }

 private:
  // Returns true on success
  bool ReadMbr(char* buffer, int buffer_len, const char* device);

  // Returns true on success
  bool WriteMbr(const char* buffer, int buffer_len, const char* device);

  DISALLOW_COPY_AND_ASSIGN(SetBootableFlagAction);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_SET_BOOTABLE_FLAG_ACTION_H__
