// Copyright (c) 2016 The CoreOS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_KERNEL_VERIFIER_ACTION_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_KERNEL_VERIFIER_ACTION_H__

#include <string>

#include "update_engine/action.h"
#include "update_engine/install_plan.h"

namespace chromeos_update_engine {

class KernelVerifierAction;

template<>
class ActionTraits<KernelVerifierAction> {
 public:
  // Takes the install plan as input
  typedef InstallPlan InputObjectType;
  // Passes the install plan as output
  typedef InstallPlan OutputObjectType;
};

class KernelVerifierAction : public Action<KernelVerifierAction> {
 public:
  KernelVerifierAction() {}

  typedef ActionTraits<KernelVerifierAction>::InputObjectType
  InputObjectType;
  typedef ActionTraits<KernelVerifierAction>::OutputObjectType
  OutputObjectType;
  void PerformAction();
  void TerminateProcessing() {}

  // Debugging/logging
  static std::string StaticType() { return "KernelVerifierAction"; }
  std::string Type() const { return StaticType(); }

 private:
  // The install plan we're passed in via the input pipe.
  InstallPlan install_plan_;

  DISALLOW_COPY_AND_ASSIGN(KernelVerifierAction);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_KERNEL_VERIFIER_ACTION_H__
