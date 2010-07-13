// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/omaha_response_handler_action.h"
#include <string>
#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

namespace {
// If the file part of the download URL contains kFullUpdateTag, then and
// only then do we assume it's a full update. Otherwise, we assume it's a
// delta update.
const string kFullUpdateTag = "_FULL_";
}  // namespace

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
  TEST_AND_RETURN(GetInstallDev(
      (!boot_device_.empty() ? boot_device_ : utils::BootDevice()),
      &install_plan_.install_path));
  install_plan_.kernel_install_path =
      utils::BootKernelDevice(install_plan_.install_path);

  install_plan_.is_full_update = true;  // TODO(adlr): know if update is a delta

  TEST_AND_RETURN(HasOutputPipe());
  if (HasOutputPipe())
    SetOutputObject(install_plan_);
  LOG(INFO) << "Using this install plan:";
  install_plan_.Dump();

  completer.set_success(true);
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
