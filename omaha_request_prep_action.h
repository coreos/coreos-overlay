// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_RESQUEST_PREP_ACTION_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_RESQUEST_PREP_ACTION_H__

#include <string>
#include "update_engine/action.h"
#include "update_engine/update_check_action.h"

// This gathers local system information and prepares info used by the
// update check action.

namespace chromeos_update_engine {

class OmahaRequestPrepAction;
class NoneType;

template<>
class ActionTraits<OmahaRequestPrepAction> {
 public:
  typedef NoneType InputObjectType;
  typedef UpdateCheckParams OutputObjectType;
};

class OmahaRequestPrepAction : public Action<OmahaRequestPrepAction> {
 public:
  explicit OmahaRequestPrepAction(bool force_full_update)
      : force_full_update_(force_full_update) {}
  typedef ActionTraits<OmahaRequestPrepAction>::InputObjectType
      InputObjectType;
  typedef ActionTraits<OmahaRequestPrepAction>::OutputObjectType
      OutputObjectType;
  void PerformAction();

  // This is a synchronous action, and thus TerminateProcessing() should
  // never be called
  void TerminateProcessing() { CHECK(false); }

  // Debugging/logging
  static std::string StaticType() { return "OmahaRequestPrepAction"; }
  std::string Type() const { return StaticType(); }

  // For unit-tests.
  void set_root(const std::string& root) {
    root_ = root;
  }

 private:
  // Gets a machine-local ID (for now, first MAC address we find)
  std::string GetMachineId() const;

  // Fetches the value for a given key from
  // /mnt/stateful_partition/etc/lsb-release if possible. Failing that,
  // it looks for the key in /etc/lsb-release .
  std::string GetLsbValue(const std::string& key) const;

  // Gets the machine type (e.g. "i686")
  std::string GetMachineType() const;

  // Set to true if this should set up the Update Check Action to do
  // a full update
  bool force_full_update_;

  // When reading files, prepend root_ to the paths. Useful for testing.
  std::string root_;

  DISALLOW_COPY_AND_ASSIGN(OmahaRequestPrepAction);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_RESQUEST_PREP_ACTION_H__
