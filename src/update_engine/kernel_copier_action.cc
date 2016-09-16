// Copyright (c) 2016 The CoreOS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/kernel_copier_action.h"

#include <string>

#include "files/file_util.h"
#include "update_engine/utils.h"
#include "update_engine/omaha_hash_calculator.h"

using std::string;

namespace chromeos_update_engine {

void KernelCopierAction::PerformAction() {
  // Will tell the ActionProcessor we've failed if we return.
  ScopedActionCompleter abort_action_completer(processor_, this);

  if (!HasInputObject()) {
    LOG(ERROR) << "KernelCopierAction missing input object.";
    return;
  }
  install_plan_ = GetInputObject();
  if (install_plan_.is_resume || install_plan_.kernel_path.empty()) {
    // Resuming download or no kernel to install, no copy needed.
    if (HasOutputPipe())
      SetOutputObject(install_plan_);
    abort_action_completer.set_code(kActionCodeSuccess);
    return;
  }

  // Full updates can still apply if the old kernel is missing for some
  // reason. Just issue a warning and move on. The payload processor will
  // reject delta updates with a hash mismatch if this happens.
  const string source = install_plan_.old_kernel_path;
  if (!utils::FileExists(source.c_str())) {
    LOG(WARNING) << "Missing source kernel " << source;
    if (HasOutputPipe())
      SetOutputObject(install_plan_);
    abort_action_completer.set_code(kActionCodeSuccess);
    return;
  }

  off_t length = utils::FileSize(source);
  if (length < 0) {
    LOG(ERROR) << "Failed to determine size of source kernel " << source;
    return;
  }

  if (length != OmahaHashCalculator::RawHashOfFile(
        source, length, &install_plan_.old_kernel_hash)) {
    LOG(ERROR) << "Failed to compute hash of source kernel " << source;
    return;
  }

  if (!files::CopyFile(files::FilePath(source),
                       files::FilePath(install_plan_.kernel_path))) {
    LOG(ERROR) << "Failed to copy kernel from " << source << " to "
               << install_plan_.kernel_path;
    return;
  }

  // Success! Pass along the new install_plan to the next action.
  if (HasOutputPipe())
    SetOutputObject(install_plan_);
  abort_action_completer.set_code(kActionCodeSuccess);
  return;
}

}  // namespace chromeos_update_engine
