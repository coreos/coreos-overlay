// Copyright (c) 2016 The CoreOS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_KERNEL_COPIER_ACTION_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_KERNEL_COPIER_ACTION_H__

#include <string>

#include "update_engine/action.h"
#include "update_engine/install_plan.h"

namespace chromeos_update_engine {

class KernelCopierAction;

template<>
class ActionTraits<KernelCopierAction> {
 public:
  // Takes the install plan as input
  typedef InstallPlan InputObjectType;
  // Passes the install plan as output
  typedef InstallPlan OutputObjectType;
};

class KernelCopierAction : public Action<KernelCopierAction> {
 public:
  KernelCopierAction() {}

  typedef ActionTraits<KernelCopierAction>::InputObjectType
  InputObjectType;
  typedef ActionTraits<KernelCopierAction>::OutputObjectType
  OutputObjectType;
  void PerformAction();
  void TerminateProcessing() {}

  // Used for testing, so we can copy from somewhere other than root
  void set_copy_source(const std::string& path) { copy_source_ = path; }

  // Debugging/logging
  static std::string StaticType() { return "KernelCopierAction"; }
  std::string Type() const { return StaticType(); }

 private:
  // The path to copy from. If empty (the default) it is auto-detected.
  std::string copy_source_;

  // The install plan we're passed in via the input pipe.
  InstallPlan install_plan_;

  DISALLOW_COPY_AND_ASSIGN(KernelCopierAction);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_KERNEL_COPIER_ACTION_H__
