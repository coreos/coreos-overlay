// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_PARAMS_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_PARAMS_H__

#include <string>

#include <base/basictypes.h>
#include <base/time.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

// This gathers local system information and prepares info used by the
// Omaha request action.

namespace chromeos_update_engine {

// The default "official" Omaha update URL.
extern const char* const kProductionOmahaUrl;

// This struct encapsulates the data Omaha gets for the request, along with
// essential state needed for the processing of the request/response.
// The strings in this struct should not be XML escaped.
// TODO (jaysri): Consider renaming this to reflect its lifetime more
// appropriately.
struct OmahaRequestParams {

  OmahaRequestParams()
      : os_platform(kOsPlatform),
        os_version(kOsVersion),
        app_id(kAppId),
        delta_okay(true),
        update_disabled(false),
        wall_clock_based_wait_enabled(false),
        update_check_count_wait_enabled(false),
        min_update_checks_needed(kDefaultMinUpdateChecks),
        max_update_checks_allowed(kDefaultMaxUpdateChecks) {}

  OmahaRequestParams(const std::string& in_os_platform,
                     const std::string& in_os_version,
                     const std::string& in_os_sp,
                     const std::string& in_os_board,
                     const std::string& in_app_id,
                     const std::string& in_app_version,
                     const std::string& in_app_lang,
                     const std::string& in_app_track,
                     const std::string& in_hardware_class,
                     bool in_delta_okay,
                     const std::string& in_update_url,
                     bool in_update_disabled,
                     const std::string& in_target_version_prefix)
      : os_platform(in_os_platform),
        os_version(in_os_version),
        os_sp(in_os_sp),
        os_board(in_os_board),
        app_id(in_app_id),
        app_version(in_app_version),
        app_lang(in_app_lang),
        app_track(in_app_track),
        hardware_class(in_hardware_class),
        delta_okay(in_delta_okay),
        update_url(in_update_url),
        update_disabled(in_update_disabled),
        target_version_prefix(in_target_version_prefix),
        wall_clock_based_wait_enabled(false),
        update_check_count_wait_enabled(false),
        min_update_checks_needed(kDefaultMinUpdateChecks),
        max_update_checks_allowed(kDefaultMaxUpdateChecks) {}

  std::string os_platform;
  std::string os_version;
  std::string os_sp;
  std::string os_board;
  std::string app_id;
  std::string app_version;
  std::string app_lang;
  std::string app_track;
  std::string hardware_class;  // Hardware Qualification ID of the client
  bool delta_okay;  // If this client can accept a delta

  std::string update_url;

  static const char kUpdateTrackKey[];

  bool update_disabled;
  std::string target_version_prefix;

  bool wall_clock_based_wait_enabled;
  base::TimeDelta waiting_period;

  bool update_check_count_wait_enabled;
  int64 min_update_checks_needed;
  int64 max_update_checks_allowed;

  // Suggested defaults
  static const char* const kAppId;
  static const char* const kOsPlatform;
  static const char* const kOsVersion;
  static const char* const kUpdateUrl;
  static const int64 kDefaultMinUpdateChecks = 0;
  static const int64 kDefaultMaxUpdateChecks = 8;
};

class OmahaRequestDeviceParams : public OmahaRequestParams {
 public:
  OmahaRequestDeviceParams();

  // Initializes all the data in the object. Non-empty
  // |in_app_version| or |in_update_url| prevents automatic detection
  // of the parameter. Returns true on success, false otherwise.
  bool Init(const std::string& in_app_version,
            const std::string& in_update_url,
            const std::string& in_release_track);

  // Permanently changes the release track to |track|. Returns true on success,
  // false otherwise.
  bool SetTrack(const std::string& track);
  static bool SetDeviceTrack(const std::string& track);

  // Returns the release track. On error, returns an empty string.
  static std::string GetDeviceTrack();

  // For unit-tests.
  void set_root(const std::string& root) { root_ = root; }

  // Enforce security mode for testing purposes.
  void SetLockDown(bool lock);

 private:
  FRIEND_TEST(OmahaRequestDeviceParamsTest, IsValidTrackTest);
  FRIEND_TEST(OmahaRequestDeviceParamsTest, ShouldLockDownTest);

  // Use a validator that is a non-static member of this class so that its
  // inputs can be mocked in unit tests (e.g., build type for IsValidTrack).
  typedef bool(OmahaRequestDeviceParams::*ValueValidator)(
      const std::string&) const;

  // Returns true if parameter values should be locked down for security
  // reasons. If this is an official build running in normal boot mode, all
  // values except the release track are parsed only from the read-only rootfs
  // partition and the track values are restricted to a pre-approved set.
  bool ShouldLockDown() const;

  // Returns true if |track| is a valid track, false otherwise. This method
  // restricts the track value only if the image is official (see
  // IsOfficialBuild).
  bool IsValidTrack(const std::string& track) const;

  // Fetches the value for a given key from
  // /mnt/stateful_partition/etc/lsb-release if possible and |stateful_override|
  // is true. Failing that, it looks for the key in /etc/lsb-release. If
  // |validator| is non-NULL, uses it to validate and ignore invalid valies.
  std::string GetLsbValue(const std::string& key,
                          const std::string& default_value,
                          ValueValidator validator,
                          bool stateful_override) const;

  // Gets the machine type (e.g. "i686").
  std::string GetMachineType() const;

  // When reading files, prepend root_ to the paths. Useful for testing.
  std::string root_;

  // Force security lock down for testing purposes.
  bool force_lock_down_;
  bool forced_lock_down_;

  DISALLOW_COPY_AND_ASSIGN(OmahaRequestDeviceParams);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_PARAMS_H__
