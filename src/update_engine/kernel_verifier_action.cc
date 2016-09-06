// Copyright (c) 2016 The CoreOS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/kernel_verifier_action.h"

#include <string>

#include "update_engine/utils.h"
#include "update_engine/omaha_hash_calculator.h"

using std::string;

namespace chromeos_update_engine {

void KernelVerifierAction::PerformAction() {
  // Will tell the ActionProcessor we've failed if we return.
  ScopedActionCompleter abort_action_completer(processor_, this);
  abort_action_completer.set_code(kActionCodeNewKernelVerificationError);

  if (!HasInputObject()) {
    LOG(ERROR) << "KernelVerifierAction missing input object.";
    return;
  }
  install_plan_ = GetInputObject();

  // If unset then a new kernel was never installed.
  if (install_plan_.new_kernel_size == 0 &&
      install_plan_.new_kernel_hash.empty()) {
    if (HasOutputPipe())
      SetOutputObject(install_plan_);
    abort_action_completer.set_code(kActionCodeSuccess);
    return;
  }

  off_t length = utils::FileSize(install_plan_.kernel_path);
  if (length < 0) {
    LOG(ERROR) << "Failed to determine size of kernel "
               << install_plan_.kernel_path;
    return;
  }

  if (static_cast<uint64_t>(length) != install_plan_.new_kernel_size) {
    LOG(ERROR) << "Kernel verification failure: "
               << install_plan_.kernel_path << " is "
               << length << " bytes. Expected "
               << install_plan_.new_kernel_size;
    return;
  }

  OmahaHashCalculator hasher;
  if (length != hasher.UpdateFile(install_plan_.kernel_path, length) ||
      !hasher.Finalize()) {
    LOG(ERROR) << "Failed to compute hash of kernel "
               << install_plan_.kernel_path;
    return;
  }

  if (hasher.raw_hash() != install_plan_.new_kernel_hash) {
    string expected_hash;
    if (!OmahaHashCalculator::Base64Encode(
          install_plan_.new_kernel_hash.data(),
          install_plan_.new_kernel_hash.size(),
          &expected_hash)) {
      expected_hash = "<unknown>";
    }
    LOG(ERROR) << "Kernel verification failure: "
               << install_plan_.kernel_path
               << "\nBad hash: " << hasher.hash()
               << "\nExpected: " << expected_hash;
    return;
  }

  LOG(INFO) << "Kernel size: " << length;
  LOG(INFO) << "Kernel hash: " << hasher.hash();

  // Success! Pass along the new install_plan to the next action.
  if (HasOutputPipe())
    SetOutputObject(install_plan_);
  abort_action_completer.set_code(kActionCodeSuccess);
  return;
}

}  // namespace chromeos_update_engine
