// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/omaha_response_handler_action.h"

#include <string>

#include <base/logging.h>

#include "update_engine/delta_performer.h"
#include "update_engine/prefs_interface.h"
#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

void OmahaResponseHandlerAction::PerformAction() {
  CHECK(HasInputObject());
  ScopedActionCompleter completer(processor_, this);
  const OmahaResponse& response = GetInputObject();
  if (!response.update_exists) {
    got_no_update_response_ = true;
    LOG(INFO) << "There are no updates. Aborting.";
    return;
  }
  install_plan_.download_url = response.codebase;
  install_plan_.size = response.size;
  install_plan_.download_hash = response.hash;

  install_plan_.is_resume =
      DeltaPerformer::CanResumeUpdate(prefs_, response.hash);
  if (!install_plan_.is_resume) {
    LOG_IF(WARNING, !DeltaPerformer::ResetUpdateProgress(prefs_))
        << "Unable to reset the update progress.";
    LOG_IF(WARNING, !prefs_->SetString(kPrefsUpdateCheckResponseHash,
                                       response.hash))
        << "Unable to save the update check response hash.";
  }

  TEST_AND_RETURN(GetInstallDev(
      (!boot_device_.empty() ? boot_device_ : utils::BootDevice()),
      &install_plan_.install_path));
  install_plan_.kernel_install_path =
      utils::BootKernelDevice(install_plan_.install_path);

  install_plan_.is_full_update = !response.is_delta;

  TEST_AND_RETURN(HasOutputPipe());
  if (HasOutputPipe())
    SetOutputObject(install_plan_);
  LOG(INFO) << "Using this install plan:";
  install_plan_.Dump();

  completer.set_code(kActionCodeSuccess);
}

bool OmahaResponseHandlerAction::GetInstallDev(const std::string& boot_dev,
                                               std::string* install_dev) {
  TEST_AND_RETURN_FALSE(utils::StringHasPrefix(boot_dev, "/dev/"));
  string ret(boot_dev);
  string::reverse_iterator it = ret.rbegin();  // last character in string
  // Right now, we just switch '3' and '5' partition numbers.
  TEST_AND_RETURN_FALSE((*it == '3') || (*it == '5'));
  *it = (*it == '3') ? '5' : '3';
  *install_dev = ret;
  return true;
}

}  // namespace chromeos_update_engine
