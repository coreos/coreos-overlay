// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/omaha_request_params.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/utsname.h>

#include <map>
#include <string>

#include <base/file_util.h>
#include <base/string_util.h>

#include "update_engine/simple_key_value_store.h"
#include "update_engine/utils.h"

#define CALL_MEMBER_FN(object, member) ((object).*(member))

using std::map;
using std::string;

namespace chromeos_update_engine {

const char OmahaRequestParams::kUpdateTrackKey[] = "CHROMEOS_RELEASE_TRACK";

const char* const OmahaRequestParams::kAppId(
    "{87efface-864d-49a5-9bb3-4b050a7c227a}");
const char* const OmahaRequestParams::kOsPlatform("Chrome OS");
const char* const OmahaRequestParams::kOsVersion("Indy");
const char* const OmahaRequestParams::kUpdateUrl(
    "https://tools.google.com/service/update2");

static const char kHWIDPath[] = "/sys/devices/platform/chromeos_acpi/HWID";

OmahaRequestDeviceParams::OmahaRequestDeviceParams() :
    force_build_type_(false),
    forced_official_build_(false) {}

bool OmahaRequestDeviceParams::Init(const std::string& in_app_version,
                                    const std::string& in_update_url) {
  os_platform = OmahaRequestParams::kOsPlatform;
  os_version = OmahaRequestParams::kOsVersion;
  app_version = in_app_version.empty() ?
      GetLsbValue("CHROMEOS_RELEASE_VERSION", "", NULL) : in_app_version;
  os_sp = app_version + "_" + GetMachineType();
  os_board = GetLsbValue("CHROMEOS_RELEASE_BOARD", "", NULL);
  app_id = OmahaRequestParams::kAppId;
  app_lang = "en-US";
  app_track = GetLsbValue(
      kUpdateTrackKey,
      "",
      &chromeos_update_engine::OmahaRequestDeviceParams::IsValidTrack);
  hardware_class = GetHardwareClass();
  struct stat stbuf;

  // Deltas are only okay if the /.nodelta file does not exist.
  // If we don't know (i.e. stat() returns some unexpected error),
  // then err on the side of caution and say deltas are not okay
  delta_okay = (stat((root_ + "/.nodelta").c_str(), &stbuf) < 0) &&
               (errno == ENOENT);

  update_url = in_update_url.empty() ?
      GetLsbValue("CHROMEOS_AUSERVER", OmahaRequestParams::kUpdateUrl, NULL) :
      in_update_url;
  return true;
}

bool OmahaRequestDeviceParams::SetTrack(const std::string& track) {
  TEST_AND_RETURN_FALSE(IsValidTrack(track));
  FilePath kFile(root_ + utils::kStatefulPartition + "/etc/lsb-release");
  string file_data;
  map<string, string> data;
  if (file_util::ReadFileToString(kFile, &file_data)) {
    data = simple_key_value_store::ParseString(file_data);
  }
  data[kUpdateTrackKey] = track;
  file_data = simple_key_value_store::AssembleString(data);
  TEST_AND_RETURN_FALSE(file_util::CreateDirectory(kFile.DirName()));
  TEST_AND_RETURN_FALSE(
      file_util::WriteFile(kFile, file_data.data(), file_data.size()) ==
      static_cast<int>(file_data.size()));
  app_track = track;
  return true;
}

bool OmahaRequestDeviceParams::SetDeviceTrack(const std::string& track) {
  OmahaRequestDeviceParams params;
  TEST_AND_RETURN_FALSE(params.Init("", ""));
  return params.SetTrack(track);
}

string OmahaRequestDeviceParams::GetLsbValue(const string& key,
                                             const string& default_value,
                                             ValueValidator validator) const {
  string files[] = {string(utils::kStatefulPartition) + "/etc/lsb-release",
                    "/etc/lsb-release"};
  for (unsigned int i = 0; i < arraysize(files); ++i) {
    // TODO(adlr): make sure files checked are owned as root (and all
    // their parents are recursively, too).
    string file_data;
    if (!utils::ReadFileToString(root_ + files[i], &file_data))
      continue;

    map<string, string> data = simple_key_value_store::ParseString(file_data);
    if (utils::MapContainsKey(data, key)) {
      const string& value = data[key];
      if (validator && !CALL_MEMBER_FN(*this, validator)(value)) {
        continue;
      }
      return value;
    }
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

string OmahaRequestDeviceParams::GetHardwareClass() const {
  string hwid;
  if (!file_util::ReadFileToString(FilePath(root_ + kHWIDPath), &hwid)) {
    LOG(ERROR) << "Unable to determine the system hardware qualification ID.";
    return "";
  }
  TrimWhitespaceASCII(hwid, TRIM_ALL, &hwid);
  return hwid;
}

bool OmahaRequestDeviceParams::IsOfficialBuild() const {
  return force_build_type_ ? forced_official_build_ : utils::IsOfficialBuild();
}

bool OmahaRequestDeviceParams::IsValidTrack(const std::string& track) const {
  return IsOfficialBuild() ?
      (track == "beta-channel" || track == "dev-channel") : true;
}

void OmahaRequestDeviceParams::SetBuildTypeOfficial(bool is_official) {
  force_build_type_ = true;
  forced_official_build_ = is_official;
}

}  // namespace chromeos_update_engine
