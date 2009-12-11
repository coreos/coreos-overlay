// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_RESPONSE_HANDLER_ACTION_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_RESPONSE_HANDLER_ACTION_H__

#include <string>
#include "update_engine/action.h"
#include "update_engine/install_plan.h"
#include "update_engine/update_check_action.h"

// This class reads in an Omaha response and converts what it sees into
// an install plan which is passed out.

namespace chromeos_update_engine {

class OmahaResponseHandlerAction;

template<>
class ActionTraits<OmahaResponseHandlerAction> {
 public:
  typedef UpdateCheckResponse InputObjectType;
  typedef InstallPlan OutputObjectType;
};

class OmahaResponseHandlerAction : public Action<OmahaResponseHandlerAction> {
 public:
  OmahaResponseHandlerAction() : got_no_update_response_(false) {}
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
    boot_device_ = boot_device;
  }
  
  bool GotNoUpdateResponse() const { return got_no_update_response_; }

  // Debugging/logging
  static std::string StaticType() { return "OmahaResponseHandlerAction"; }
  std::string Type() const { return StaticType(); }

 private:
  // Assumes you want to install on the "other" device, where the other
  // device is what you get if you swap 1 for 2 or 3 for 4 or vice versa
  // for the number at the end of the boot device. E.g., /dev/sda1 -> /dev/sda2
  // or /dev/sda4 -> /dev/sda3
  static bool GetInstallDev(const std::string& boot_dev,
                            std::string* install_dev);

  // set to non-empty in unit tests
  std::string boot_device_;
  
  // True only if we got a response and the response said no updates
  bool got_no_update_response_;

  DISALLOW_COPY_AND_ASSIGN(OmahaResponseHandlerAction);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_RESPONSE_HANDLER_ACTION_H__
