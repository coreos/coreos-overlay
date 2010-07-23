// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_PARAMS_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_PARAMS_H__

#include <string>

#include "base/basictypes.h"

// This gathers local system information and prepares info used by the
// Omaha request action.

namespace chromeos_update_engine {

// This struct encapsulates the data Omaha gets for the request.
// These strings in this struct should not be XML escaped.
struct OmahaRequestParams {
  OmahaRequestParams()
      : os_platform(kOsPlatform), os_version(kOsVersion), app_id(kAppId) {}
  OmahaRequestParams(const std::string& in_machine_id,
                     const std::string& in_user_id,
                     const std::string& in_os_platform,
                     const std::string& in_os_version,
                     const std::string& in_os_sp,
                     const std::string& in_os_board,
                     const std::string& in_app_id,
                     const std::string& in_app_version,
                     const std::string& in_app_lang,
                     const std::string& in_app_track,
                     const bool in_delta_okay,
                     const std::string& in_update_url)
      : machine_id(in_machine_id),
        user_id(in_user_id),
        os_platform(in_os_platform),
        os_version(in_os_version),
        os_sp(in_os_sp),
        os_board(in_os_board),
        app_id(in_app_id),
        app_version(in_app_version),
        app_lang(in_app_lang),
        app_track(in_app_track),
        delta_okay(in_delta_okay),
        update_url(in_update_url) {}

  std::string machine_id;
  std::string user_id;
  std::string os_platform;
  std::string os_version;
  std::string os_sp;
  std::string os_board;
  std::string app_id;
  std::string app_version;
  std::string app_lang;
  std::string app_track;
  bool delta_okay;  // If this client can accept a delta

  std::string update_url;

  // Suggested defaults
  static const char* const kAppId;
  static const char* const kOsPlatform;
  static const char* const kOsVersion;
  static const char* const kUpdateUrl;
};

class OmahaRequestDeviceParams : public OmahaRequestParams {
 public:
  OmahaRequestDeviceParams() {}

  // Initializes all the data in the object. Non-empty
  // |in_app_version| or |in_update_url| prevents automatic detection
  // of the parameter. Returns true on success, false otherwise.
  bool Init(const std::string& in_app_version,
            const std::string& in_update_url);

  // For unit-tests.
  void set_root(const std::string& root) { root_ = root; }

 private:
  // Gets a machine-local ID (for now, first MAC address we find).
  bool GetMachineId(std::string* out_id) const;

  // Fetches the value for a given key from
  // /mnt/stateful_partition/etc/lsb-release if possible. Failing that,
  // it looks for the key in /etc/lsb-release.
  std::string GetLsbValue(const std::string& key,
                          const std::string& default_value) const;

  // Gets the machine type (e.g. "i686").
  std::string GetMachineType() const;

  // When reading files, prepend root_ to the paths. Useful for testing.
  std::string root_;

  DISALLOW_COPY_AND_ASSIGN(OmahaRequestDeviceParams);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_PARAMS_H__
