// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/omaha_request_prep_action.h"
#include <sys/utsname.h>
#include <errno.h>
#include <string>
#include "base/string_util.h"
#include "update_engine/utils.h"

using std::string;

// This gathers local system information and prepares info used by the
// update check action.

namespace {
const string OmahaIdPath() {
  return chromeos_update_engine::utils::kStatefulPartition + "/etc/omaha_id";
}
}  // namespace {}

namespace chromeos_update_engine {

void OmahaRequestPrepAction::PerformAction() {
  // TODO(adlr): honor force_full_update_
  ScopedActionCompleter completer(processor_, this);
  string machine_id;
  TEST_AND_RETURN(GetMachineId(&machine_id));
  const string version(GetLsbValue("GOOGLE_RELEASE"));
  const string sp(version + "_" + GetMachineType());
  const string track(GetLsbValue("GOOGLE_TRACK"));

  UpdateCheckParams out(machine_id,  // machine_id
                        machine_id,  // user_id (use machine_id)
                        UpdateCheckParams::kOsPlatform,
                        UpdateCheckParams::kOsVersion,
                        sp,  // e.g. 0.2.3.3_i686
                        UpdateCheckParams::kAppId,
                        version,  // app version (from lsb-release)
                        "en-US",  //lang
                        track);  // track

  CHECK(HasOutputPipe());
  SetOutputObject(out);
  completer.set_success(true);
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
bool OmahaRequestPrepAction::GetMachineId(std::string* out_id) const {
  // See if we have an existing Machine ID
  const string omaha_id_path = root_ + OmahaIdPath();
  
  if (utils::ReadFileToString(omaha_id_path, out_id) &&
      out_id->size() == kGuidStringLength) {
    return true;
  }
  
  // Create a new ID
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

std::string OmahaRequestPrepAction::GetLsbValue(const std::string& key) const {
  string files[] = {utils::kStatefulPartition + "/etc/lsb-release",
                    "/etc/lsb-release"};
  for (unsigned int i = 0; i < arraysize(files); i++) {
    string file_data;
    if (!utils::ReadFileToString(root_ + files[i], &file_data))
      continue;
    string::size_type pos = 0;
    if (!utils::StringHasPrefix(file_data, key + "=")) {
      pos = file_data.find(string("\n") + key + "=");
      if (pos != string::npos)
        pos++;  // advance past \n
    }
    if (pos == string::npos)
      continue;
    pos += key.size() + 1;  // advance past the key and the '='
    string::size_type endpos = file_data.find('\n', pos);
    string::size_type length =
        (endpos == string::npos ? string::npos : endpos - pos);
    return file_data.substr(pos, length);
  }
  // not found
  return "";
}

std::string OmahaRequestPrepAction::GetMachineType() const {
  struct utsname buf;
  string ret;
  if (uname(&buf) == 0)
    ret = buf.machine;
  return ret;
}

}  // namespace chromeos_update_engine
