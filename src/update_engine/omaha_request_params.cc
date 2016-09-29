// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/omaha_request_params.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/utsname.h>

#include <map>
#include <string>
#include <vector>

#include "update_engine/simple_key_value_store.h"
#include "update_engine/system_state.h"
#include "update_engine/prefs_interface.h"
#include "update_engine/utils.h"

#define CALL_MEMBER_FN(object, member) ((object).*(member))

using std::map;
using std::string;
using std::vector;

namespace chromeos_update_engine {

const char* const OmahaRequestParams::kAppId(
    "{e96281a6-d1af-4bde-9a0a-97b76e56dc57}");
const char* const OmahaRequestParams::kOsPlatform("CoreOS");
const char* const OmahaRequestParams::kOsVersion("Chateau");
const char* const OmahaRequestParams::kDefaultChannel("stable");
const char* const kProductionOmahaUrl(
    "https://public.update.core-os.net/v1/update/");

bool OmahaRequestParams::Init(bool interactive) {
  os_platform_ = OmahaRequestParams::kOsPlatform;
  os_version_ = OmahaRequestParams::kOsVersion;
  oemid_ = GetOemValue("ID", "");
  oemversion_ = GetOemValue("VERSION_ID", "");
  app_version_ = GetConfValue("COREOS_RELEASE_VERSION", "");

  if (!system_state_->prefs()->GetString(kPrefsAlephVersion, &alephversion_)) {
    alephversion_.assign(app_version_);
    system_state_->prefs()->SetString(kPrefsAlephVersion, alephversion_);
  }

  os_sp_ = app_version_ + "_" + GetMachineType();
  os_board_ = GetConfValue("COREOS_RELEASE_BOARD", "");
  app_id_ = GetConfValue("COREOS_RELEASE_APPID", OmahaRequestParams::kAppId);
  app_lang_ = "en-US";
  bootid_ = utils::GetBootId();
  machineid_ = utils::GetMachineId();
  update_url_ = GetConfValue("SERVER", kProductionOmahaUrl);
  pcr_policy_url_ = GetConfValue("PCR_POLICY_SERVER", "");
  interactive_ = interactive;

  app_channel_ = GetConfValue("GROUP", kDefaultChannel);
  LOG(INFO) << "Current group set to " << app_channel_;

  // TODO: deltas can only be enabled if verity is active.
  delta_okay_ = false;

  return true;
}

string OmahaRequestParams::SearchConfValue(const vector<string>& files,
                                           const string& key,
                                           const string& default_value) const {
  for (vector<string>::const_iterator it = files.begin();
       it != files.end(); ++it) {
    string file_data;
    if (!utils::ReadFile(root_ + *it, &file_data))
      continue;

    map<string, string> data = simple_key_value_store::ParseString(file_data);
    if (data.count(key)) {
      const string& value = data[key];
      return value;
    }
  }
  // not found
  return default_value;
}

string OmahaRequestParams::GetConfValue(const string& key,
                                        const string& default_value) const {
  vector<string> files;
  files.push_back("/etc/coreos/update.conf");
  files.push_back("/usr/share/coreos/update.conf");
  files.push_back("/usr/share/coreos/release");
  return SearchConfValue(files, key, default_value);
}

string OmahaRequestParams::GetOemValue(const string& key,
                                       const string& default_value) const {
  vector<string> files;
  files.push_back("/etc/oem-release");
  files.push_back("/usr/share/oem/oem-release");
  return SearchConfValue(files, key, default_value);
}

string OmahaRequestParams::GetMachineType() const {
  struct utsname buf;
  string ret;
  if (uname(&buf) == 0)
    ret = buf.machine;
  return ret;
}

void OmahaRequestParams::set_root(const std::string& root) {
  root_ = root;
  Init(false);
}

}  // namespace chromeos_update_engine
