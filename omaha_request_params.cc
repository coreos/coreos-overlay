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

#include <base/file_util.h>
#include <policy/device_policy.h>

#include "update_engine/simple_key_value_store.h"
#include "update_engine/system_state.h"
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
const char* const kProductionOmahaUrl(
    "https://api.core-os.net/v1/update/");

const char* const OmahaRequestParams::kUpdateChannelKey(
    "CHROMEOS_RELEASE_TRACK");
const char* const OmahaRequestParams::kIsPowerwashAllowedKey(
    "CHROMEOS_IS_POWERWASH_ALLOWED");

const char* kChannelsByStability[] = {
    // This list has to be sorted from least stable to most stable channel.
    "canary-channel",
    "dev-channel",
    "beta-channel",
    "stable-channel",
};

bool OmahaRequestParams::Init(const std::string& in_app_version,
                              const std::string& in_update_url,
                              bool in_interactive) {
  InitFromLsbValue();
  bool stateful_override = !ShouldLockDown();
  os_platform_ = OmahaRequestParams::kOsPlatform;
  os_version_ = OmahaRequestParams::kOsVersion;
  app_version_ = in_app_version.empty() ?
      GetLsbValue("CHROMEOS_RELEASE_VERSION", "", NULL, stateful_override) :
      in_app_version;
  os_sp_ = app_version_ + "_" + GetMachineType();
  os_board_ = GetLsbValue("CHROMEOS_RELEASE_BOARD",
                          "",
                          NULL,
                          stateful_override);
  app_id_ = GetLsbValue("CHROMEOS_RELEASE_APPID",
                        OmahaRequestParams::kAppId,
                        NULL,
                        stateful_override);
  board_app_id_ = GetLsbValue("CHROMEOS_BOARD_APPID",
                              app_id_,
                              NULL,
                              stateful_override);
  app_lang_ = "en-US";
  hwid_ = utils::GetHardwareClass();
  bootid_ = utils::GetBootId();

  if (current_channel_ == target_channel_) {
    // deltas are only okay if the /.nodelta file does not exist.  if we don't
    // know (i.e. stat() returns some unexpected error), then err on the side of
    // caution and say deltas are not okay.
    struct stat stbuf;
    delta_okay_ = (stat((root_ + "/.nodelta").c_str(), &stbuf) < 0) &&
                  (errno == ENOENT);

  } else {
    LOG(INFO) << "Disabling deltas as a channel change is pending";
    // For now, disable delta updates if the current channel is different from
    // the channel that we're sending to the update server because such updates
    // are destined to fail -- the current rootfs hash will be different than
    // the expected hash due to the different channel in /etc/lsb-release.
    delta_okay_ = false;
  }

  if (in_update_url.empty())
    update_url_ = GetLsbValue("CHROMEOS_AUSERVER", kProductionOmahaUrl, NULL,
                              stateful_override);
  else
    update_url_ = in_update_url;

  // Set the interactive flag accordingly.
  interactive_ = in_interactive;
  return true;
}

bool OmahaRequestParams::SetTargetChannel(const std::string& new_target_channel,
                                          bool is_powerwash_allowed) {
  LOG(INFO) << "SetTargetChannel called with " << new_target_channel
            << ". Is Powerwash Allowed = "
            << utils::ToString(is_powerwash_allowed);

  // Ignore duplicate calls so we can make the method succeed and be
  // idempotent so as not to surface unnecessary errors to the UI.
  if (new_target_channel == target_channel_ &&
      is_powerwash_allowed == is_powerwash_allowed_) {
    if (new_target_channel == current_channel_) {
      // Return true to make such calls no-op and idempotent.
      LOG(INFO) << "SetTargetChannel: Already on " << current_channel_;
      return true;
    }

    LOG(INFO) << "SetTargetChannel: Target channel has already been set";
    return true;
  }

  // See if there's a channel change already in progress. If so, don't honor
  // a new channel change until the existing request is fulfilled.
  if (current_channel_ != target_channel_) {
    // Avoid dealing with multiple pending channels as they cause a lot of
    // edge cases that's not worth adding the complexity for.
    LOG(ERROR) << "Cannot change to " << new_target_channel
               << " now as we're currently in " << current_channel_
               << " and the request to change to " << target_channel_
               << " is pending";
    return false;
  }

  if (current_channel_ == "canary-channel") {
    // TODO(jaysri): chromium-os:39751: We don't have the UI warnings yet. So,
    // enable the powerwash-on-changing-to-more-stable-channel behavior for now
    // only on canary-channel devices.
    is_powerwash_allowed = true;
    LOG(INFO) << "Is Powerwash Allowed set to true as we are in canary-channel";
  } else if (!utils::IsOfficialBuild() &&
             current_channel_ == "testimage-channel") {
    // Also, allow test builds to have the powerwash behavior so we can always
    // test channel changing behavior on them, without having to first get them
    // on an official channel.
    is_powerwash_allowed = true;
    LOG(INFO) << "Is Powerwash Allowed set to true as we are running an "
                 "unofficial build";
  }

  TEST_AND_RETURN_FALSE(IsValidChannel(new_target_channel));
  FilePath kFile(root_ + utils::kStatefulPartition + "/etc/lsb-release");
  string file_data;
  map<string, string> data;
  if (file_util::ReadFileToString(kFile, &file_data)) {
    data = simple_key_value_store::ParseString(file_data);
  }
  data[kUpdateChannelKey] = new_target_channel;
  data[kIsPowerwashAllowedKey] = is_powerwash_allowed ? "true" : "false";
  file_data = simple_key_value_store::AssembleString(data);
  TEST_AND_RETURN_FALSE(file_util::CreateDirectory(kFile.DirName()));
  TEST_AND_RETURN_FALSE(
      file_util::WriteFile(kFile, file_data.data(), file_data.size()) ==
      static_cast<int>(file_data.size()));
  target_channel_ = new_target_channel;
  is_powerwash_allowed_ = is_powerwash_allowed;
  return true;
}

void OmahaRequestParams::SetTargetChannelFromLsbValue() {
  string target_channel_new_value = GetLsbValue(
      kUpdateChannelKey,
      "",
      &chromeos_update_engine::OmahaRequestParams::IsValidChannel,
      true);  // stateful_override

  if (target_channel_ != target_channel_new_value) {
    target_channel_ = target_channel_new_value;
    LOG(INFO) << "Target Channel set to " << target_channel_
              << " from LSB file";
  }
}

void OmahaRequestParams::SetCurrentChannelFromLsbValue() {
  string current_channel_new_value = GetLsbValue(
      kUpdateChannelKey,
      "",
      NULL,  // No need to validate the read-only rootfs channel.
      false);  // stateful_override is false so we get the current channel.

  if (current_channel_ != current_channel_new_value) {
    current_channel_ = current_channel_new_value;
    LOG(INFO) << "Current Channel set to " << current_channel_
              << " from LSB file in rootfs";
  }
}

void OmahaRequestParams::SetIsPowerwashAllowedFromLsbValue() {
  string is_powerwash_allowed_str = GetLsbValue(
      kIsPowerwashAllowedKey,
      "false",
      NULL, // no need to validate
      true); // always get it from stateful, as that's the only place it'll be
  bool is_powerwash_allowed_new_value = (is_powerwash_allowed_str == "true");
  if (is_powerwash_allowed_ != is_powerwash_allowed_new_value) {
    is_powerwash_allowed_ = is_powerwash_allowed_new_value;
    LOG(INFO) << "Powerwash Allowed set to "
              << utils::ToString(is_powerwash_allowed_)
              << " from LSB file in stateful";
  }
}

void OmahaRequestParams::InitFromLsbValue() {
  SetTargetChannelFromLsbValue();
  SetCurrentChannelFromLsbValue();
  SetIsPowerwashAllowedFromLsbValue();
}

string OmahaRequestParams::GetLsbValue(const string& key,
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
    if (!utils::ReadFile(root_ + *it, &file_data))
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

string OmahaRequestParams::GetMachineType() const {
  struct utsname buf;
  string ret;
  if (uname(&buf) == 0)
    ret = buf.machine;
  return ret;
}

bool OmahaRequestParams::ShouldLockDown() const {
  if (force_lock_down_) {
    return forced_lock_down_;
  }
  return utils::IsOfficialBuild() && utils::IsNormalBootMode();
}

bool OmahaRequestParams::IsValidChannel(const std::string& channel) const {
  return GetChannelIndex(channel) >= 0;
}

void OmahaRequestParams::set_root(const std::string& root) {
  root_ = root;
  InitFromLsbValue();
}

void OmahaRequestParams::SetLockDown(bool lock) {
  force_lock_down_ = true;
  forced_lock_down_ = lock;
}

int OmahaRequestParams::GetChannelIndex(const std::string& channel) const {
  for (size_t t = 0; t < arraysize(kChannelsByStability); ++t)
    if (channel == kChannelsByStability[t])
      return t;

  return -1;
}

bool OmahaRequestParams::to_more_stable_channel() const {
  int current_channel_index = GetChannelIndex(current_channel_);
  int target_channel_index = GetChannelIndex(target_channel_);

  return target_channel_index > current_channel_index;
}

}  // namespace chromeos_update_engine
