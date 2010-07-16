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

namespace {
const string OmahaIdPath() {
  return string(chromeos_update_engine::utils::kStatefulPartition) +
      "/etc/omaha_id";
}
}  // namespace {}

namespace chromeos_update_engine {

bool OmahaRequestDeviceParams::Init() {
  TEST_AND_RETURN_FALSE(GetMachineId(&machine_id));
  user_id = machine_id;
  os_platform = OmahaRequestParams::kOsPlatform;
  os_version = OmahaRequestParams::kOsVersion;
  app_version = GetLsbValue("CHROMEOS_RELEASE_VERSION", "");
  os_sp = app_version + "_" + GetMachineType();
  os_board = GetLsbValue("CHROMEOS_RELEASE_BOARD", "");
  app_id = OmahaRequestParams::kAppId;
  app_lang = "en-US";
  app_track = GetLsbValue("CHROMEOS_RELEASE_TRACK", "");
  update_url = GetLsbValue("CHROMEOS_AUSERVER",
                           OmahaRequestParams::kUpdateUrl);
  return true;
}

namespace {
const size_t kGuidDataByteLength = 128 / 8;
const string::size_type kGuidStringLength = 38;
// Formats 16 bytes (128 bits) of data as a GUID:
// "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}" where X is a hex digit
string GuidFromData(const unsigned char data[kGuidDataByteLength]) {
  return StringPrintf(
      "{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
      data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
      data[8], data[9], data[10], data[11], data[12], data[13], data[14],
      data[15]);
}
}

// Returns true on success.
bool OmahaRequestDeviceParams::GetMachineId(std::string* out_id) const {
  // Checks if we have an existing Machine ID.
  const string omaha_id_path = root_ + OmahaIdPath();

  if (utils::ReadFileToString(omaha_id_path, out_id) &&
      out_id->size() == kGuidStringLength) {
    return true;
  }

  // Creates a new ID.
  int rand_fd = open("/dev/urandom", O_RDONLY, 0);
  TEST_AND_RETURN_FALSE_ERRNO(rand_fd >= 0);
  ScopedFdCloser rand_fd_closer(&rand_fd);
  unsigned char buf[kGuidDataByteLength];
  size_t bytes_read = 0;
  while (bytes_read < sizeof(buf)) {
    ssize_t rc = read(rand_fd, buf + bytes_read, sizeof(buf) - bytes_read);
    TEST_AND_RETURN_FALSE_ERRNO(rc > 0);
    bytes_read += rc;
  }
  string guid = GuidFromData(buf);
  TEST_AND_RETURN_FALSE(
      utils::WriteFile(omaha_id_path.c_str(), guid.data(), guid.size()));
  *out_id = guid;
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
