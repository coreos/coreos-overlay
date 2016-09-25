// Copyright (c) 2016 The CoreOS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/pcr_policy_post_action.h"

#include <string>
#include <vector>

#include "update_engine/utils.h"
#include "update_engine/omaha_hash_calculator.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

void PCRPolicyPostAction::PerformAction() {
  // Will tell the ActionProcessor we've failed if we return.
  ScopedActionCompleter abort_action_completer(processor_, this);
  abort_action_completer.set_code(kActionCodeNewPCRPolicyVerificationError);

  if (!HasInputObject()) {
    LOG(ERROR) << "PCRPolicyPostAction missing input object.";
    return;
  }
  install_plan_ = GetInputObject();

  // If unset then a new pcr_policy was never installed.
  if (install_plan_.new_pcr_policy_size == 0 &&
      install_plan_.new_pcr_policy_hash.empty()) {
    if (HasOutputPipe())
      SetOutputObject(install_plan_);
    abort_action_completer.set_code(kActionCodeSuccess);
    return;
  }

  off_t length = utils::FileSize(install_plan_.pcr_policy_path);
  if (length < 0) {
    LOG(ERROR) << "Failed to determine size of "
               << install_plan_.pcr_policy_path;
    return;
  }

  if (static_cast<uint64_t>(length) != install_plan_.new_pcr_policy_size) {
    LOG(ERROR) << "PCR policy verification failure: "
               << install_plan_.pcr_policy_path << " is "
               << length << " bytes. Expected "
               << install_plan_.new_pcr_policy_size;
    return;
  }

  vector<char> data;
  if (!utils::ReadFile(install_plan_.pcr_policy_path, &data)) {
    LOG(ERROR) << "Failed to read " << install_plan_.pcr_policy_path;
    return;
  }

  OmahaHashCalculator hasher;
  if (!hasher.Update(data.data(), data.size()) || !hasher.Finalize()) {
    LOG(ERROR) << "Failed to compute hash of "
               << install_plan_.pcr_policy_path;
    return;
  }

  if (hasher.raw_hash() != install_plan_.new_pcr_policy_hash) {
    string expected_hash;
    if (!OmahaHashCalculator::Base64Encode(
          install_plan_.new_pcr_policy_hash.data(),
          install_plan_.new_pcr_policy_hash.size(),
          &expected_hash)) {
      expected_hash = "<unknown>";
    }
    LOG(ERROR) << "PCR policy verification failure: "
               << install_plan_.pcr_policy_path
               << "\nBad hash: " << hasher.hash()
               << "\nExpected: " << expected_hash;
    return;
  }

  LOG(INFO) << "PCR policy size: " << length;
  LOG(INFO) << "PCR policy hash: " << hasher.hash();

  // Success! Pass along the new install_plan to the next action.
  if (HasOutputPipe())
    SetOutputObject(install_plan_);
  abort_action_completer.set_code(kActionCodeSuccess);
  return;
}

}  // namespace chromeos_update_engine
