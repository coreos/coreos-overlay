// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_RESPONSE_HANDLER_ACTION_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_RESPONSE_HANDLER_ACTION_H__

#include <string>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/action.h"
#include "update_engine/install_plan.h"
#include "update_engine/omaha_request_action.h"
#include "update_engine/system_state.h"

// This class reads in an Omaha response and converts what it sees into
// an install plan which is passed out.

namespace chromeos_update_engine {

class OmahaResponseHandlerAction;

template<>
class ActionTraits<OmahaResponseHandlerAction> {
 public:
  typedef OmahaResponse InputObjectType;
  typedef InstallPlan OutputObjectType;
};

class OmahaResponseHandlerAction : public Action<OmahaResponseHandlerAction> {
 public:
  static const char kDeadlineFile[];

  OmahaResponseHandlerAction(SystemState* system_state);
  typedef ActionTraits<OmahaResponseHandlerAction>::InputObjectType
      InputObjectType;
  typedef ActionTraits<OmahaResponseHandlerAction>::OutputObjectType
      OutputObjectType;
  void PerformAction();

  // This is a synchronous action, and thus TerminateProcessing() should
  // never be called
  void TerminateProcessing() { CHECK(false); }

  // For unit-testing
  void set_boot_device(const std::string& boot_device) {
    install_plan_.old_partition_path = boot_device;
  }

  bool GotNoUpdateResponse() const { return got_no_update_response_; }
  const InstallPlan& install_plan() const { return install_plan_; }

  // Debugging/logging
  static std::string StaticType() { return "OmahaResponseHandlerAction"; }
  std::string Type() const { return StaticType(); }
  void set_key_path(const std::string& path) { key_path_ = path; }

 private:
  FRIEND_TEST(UpdateAttempterTest, CreatePendingErrorEventResumedTest);

  // Assumes you want to install on the "other" device, where the other
  // device is what you get if you swap 3 for 4 or vice versa for the
  // number at the end of the boot device. E.g., /dev/sda4 -> /dev/sda3
  static bool GetInstallDev(const std::string& boot_dev,
                            std::string* install_dev);

  // Selects the kernel path associated with the given partition path.
  // E.g., /dev/sda4 -> /boot/coreos/vmlinuz-a
  static bool GetKernelPath(const std::string& part_path,
                            std::string* kernel_path);

  // Global system context.
  SystemState* system_state_;

  // The install plan, if we have an update.
  InstallPlan install_plan_;

  // True only if we got a response and the response said no updates
  bool got_no_update_response_;

  // Public key path to use for payload verification.
  std::string key_path_;

  DISALLOW_COPY_AND_ASSIGN(OmahaResponseHandlerAction);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_RESPONSE_HANDLER_ACTION_H__
