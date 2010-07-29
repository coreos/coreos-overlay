// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/omaha_request_params.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/utsname.h>

#include <map>
#include <string>

#include "base/string_util.h"
#include "update_engine/simple_key_value_store.h"
#include "update_engine/utils.h"

using std::map;
using std::string;

namespace chromeos_update_engine {

const char* const OmahaRequestParams::kAppId(
    "{87efface-864d-49a5-9bb3-4b050a7c227a}");
const char* const OmahaRequestParams::kOsPlatform("Chrome OS");
const char* const OmahaRequestParams::kOsVersion("Indy");
const char* const OmahaRequestParams::kUpdateUrl(
    "https://tools.google.com/service/update2");

bool OmahaRequestDeviceParams::Init(const std::string& in_app_version,
                                    const std::string& in_update_url) {
  os_platform = OmahaRequestParams::kOsPlatform;
  os_version = OmahaRequestParams::kOsVersion;
  app_version = in_app_version.empty() ?
      GetLsbValue("CHROMEOS_RELEASE_VERSION", "") : in_app_version;
  os_sp = app_version + "_" + GetMachineType();
  os_board = GetLsbValue("CHROMEOS_RELEASE_BOARD", "");
  app_id = OmahaRequestParams::kAppId;
  app_lang = "en-US";
  app_track = GetLsbValue("CHROMEOS_RELEASE_TRACK", "");
  struct stat stbuf;

  // Deltas are only okay if the /.nodelta file does not exist.
  // If we don't know (i.e. stat() returns some unexpected error),
  // then err on the side of caution and say deltas are not okay
  delta_okay = (stat((root_ + "/.nodelta").c_str(), &stbuf) < 0) &&
               (errno == ENOENT);

  update_url = in_update_url.empty() ?
      GetLsbValue("CHROMEOS_AUSERVER", OmahaRequestParams::kUpdateUrl) :
      in_update_url;
  return true;
}

string OmahaRequestDeviceParams::GetLsbValue(
    const string& key, const string& default_value) const {
  string files[] = {string(utils::kStatefulPartition) + "/etc/lsb-release",
                    "/etc/lsb-release"};
  for (unsigned int i = 0; i < arraysize(files); ++i) {
    // TODO(adlr): make sure files checked are owned as root (and all
    // their parents are recursively, too).
    string file_data;
    if (!utils::ReadFileToString(root_ + files[i], &file_data))
      continue;

    map<string, string> data = simple_key_value_store::ParseString(file_data);
    if (utils::MapContainsKey(data, key))
      return data[key];
  }
  // not found
  return default_value;
}

string OmahaRequestDeviceParams::GetMachineType() const {
  struct utsname buf;
  string ret;
  if (uname(&buf) == 0)
    ret = buf.machine;
  return ret;
}

}  // namespace chromeos_update_engine
