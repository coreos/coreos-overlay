// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/omaha_request_prep_action.h"
#include <sys/utsname.h>
#include <string>
#include "update_engine/utils.h"

using std::string;

// This gathers local system information and prepares info used by the
// update check action.

namespace chromeos_update_engine {

void OmahaRequestPrepAction::PerformAction() {
  // TODO(adlr): honor force_full_update_
  const string machine_id(GetMachineId());
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
  processor_->ActionComplete(this, true);
}

std::string OmahaRequestPrepAction::GetMachineId() const {
  FILE* fp = popen("/sbin/ifconfig", "r");
  if (!fp)
    return "";
  string data;
  for (;;) {
    char buffer[1000];
    size_t r = fread(buffer, 1, sizeof(buffer), fp);
    if (r <= 0)
      break;
    data.insert(data.end(), buffer, buffer + r);
  }
  fclose(fp);
  // scan data for MAC address
  string::size_type pos = data.find(" HWaddr ");
  if (pos == string::npos)
    return "";
  // 3 * 6 - 1 is the number of bytes of the hwaddr.
  return data.substr(pos + strlen(" HWaddr "), 3 * 6 - 1);
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
