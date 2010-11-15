// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/omaha_request_params.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/utsname.h>

#include <map>
#include <string>
#include <vector>

#include <base/file_util.h>
#include <base/string_util.h>

#include "update_engine/simple_key_value_store.h"
#include "update_engine/utils.h"

#define CALL_MEMBER_FN(object, member) ((object).*(member))

using std::map;
using std::string;
using std::vector;

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
      GetLsbValue("CHROMEOS_RELEASE_VERSION", "", NULL, true) : in_app_version;
  os_sp = app_version + "_" + GetMachineType();
  os_board = GetLsbValue("CHROMEOS_RELEASE_BOARD", "", NULL, true);
  app_id = GetLsbValue("CHROMEOS_RELEASE_APPID",
                       OmahaRequestParams::kAppId,
                       NULL,
                       true);
  app_lang = "en-US";
  app_track = GetLsbValue(
      kUpdateTrackKey,
      "",
      &chromeos_update_engine::OmahaRequestDeviceParams::IsValidTrack,
      true);
  hardware_class = GetHardwareClass();
  struct stat stbuf;

  // Deltas are only okay if the /.nodelta file does not exist.  If we don't
  // know (i.e. stat() returns some unexpected error), then err on the side of
  // caution and say deltas are not okay.
  delta_okay = (stat((root_ + "/.nodelta").c_str(), &stbuf) < 0) &&
               (errno == ENOENT);

  // For now, disable delta updates if the rootfs track is different than the
  // track that we're sending to the update server because such updates are
  // destined to fail -- the source rootfs hash will be different than the
  // expected hash due to the different track in /etc/lsb-release.
  //
  // Longer term we should consider an alternative: (a) clients can send
  // (current_version, current_channel, new_channel) information, or (b) the
  // build process can make sure releases on separate tracks are identical (i.e,
  // by not stamping the release with the channel), or (c) the release process
  // can ensure that different channels get different version numbers.
  const string rootfs_track = GetLsbValue(
      kUpdateTrackKey,
      "",
      &chromeos_update_engine::OmahaRequestDeviceParams::IsValidTrack,
      false);
  delta_okay = delta_okay && rootfs_track == app_track;

  update_url = in_update_url.empty() ?
      GetLsbValue("CHROMEOS_AUSERVER",
                  OmahaRequestParams::kUpdateUrl,
                  NULL,
                  true) :
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

string OmahaRequestDeviceParams::GetDeviceTrack() {
  OmahaRequestDeviceParams params;
  // Note that params.app_track is an empty string if the value in
  // lsb-release file is invalid. See Init() for details.
  return params.Init("", "") ? params.app_track : "";
}

string OmahaRequestDeviceParams::GetLsbValue(const string& key,
                                             const string& default_value,
                                             ValueValidator validator,
                                             bool stateful_override) const {
  vector<string> files;
  if (stateful_override) {
    files.push_back(string(utils::kStatefulPartition) + "/etc/lsb-release");
  }
  files.push_back("/etc/lsb-release");
  for (vector<string>::const_iterator it = files.begin();
       it != files.end(); ++it) {
    // TODO(adlr): make sure files checked are owned as root (and all their
    // parents are recursively, too).
    string file_data;
    if (!utils::ReadFileToString(root_ + *it, &file_data))
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
  static const char* kValidTracks[] = {
    "canary-channel",
    "beta-channel",
    "dev-channel",
  };
  if (!IsOfficialBuild()) {
    return true;
  }
  for (size_t t = 0; t < arraysize(kValidTracks); ++t) {
    if (track == kValidTracks[t]) {
      return true;
    }
  }
  return false;
}

void OmahaRequestDeviceParams::SetBuildTypeOfficial(bool is_official) {
  force_build_type_ = true;
  forced_official_build_ = is_official;
}

}  // namespace chromeos_update_engine
