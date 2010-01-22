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
  const UpdateCheckResponse& response = GetInputObject();
  if (!response.update_exists) {
    got_no_update_response_ = true;
    LOG(INFO) << "There are no updates. Aborting.";
    return;
  }
  InstallPlan install_plan;
  install_plan.download_url = response.codebase;
  install_plan.download_hash = response.hash;
  TEST_AND_RETURN(GetInstallDev(
      (!boot_device_.empty() ? boot_device_ : utils::BootDevice()),
      &install_plan.install_path));

  // Get the filename part of the url. Assume that if it has kFullUpdateTag
  // in the name, it's a full update.
  string::size_type last_slash = response.codebase.rfind('/');
  string filename;
  if (last_slash == string::npos)
    filename = response.codebase;
  else
    filename = response.codebase.substr(last_slash + 1);
  install_plan.is_full_update = (filename.find(kFullUpdateTag) != string::npos);

  if (filename.size() > 255) {
    // Very long name. Let's shorten it
    filename.resize(255);
  }
  if (HasOutputPipe())
    SetOutputObject(install_plan);
  LOG(INFO) << "Using this install plan:";
  install_plan.Dump();
  completer.set_success(true);
}

bool OmahaResponseHandlerAction::GetInstallDev(const std::string& boot_dev,
                                               std::string* install_dev) {
  TEST_AND_RETURN_FALSE(!boot_dev.empty());
  string ret(boot_dev);
  char last_char = *ret.rbegin();
  TEST_AND_RETURN_FALSE((last_char >= '0') && (last_char <= '9'));
  // Chop off last char
  ret.resize(ret.size() - 1);
  // If last_char is odd (1 or 3), increase it, otherwise decrease
  if (last_char % 2)
    last_char++;
  else
    last_char--;
  ret += last_char;
  *install_dev = ret;
  return true;
}

}  // namespace chromeos_update_engine
