// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_PARAMS_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_PARAMS_H__

#include <string>
#include <vector>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "macros.h"

// This gathers local system information and prepares info used by the
// Omaha request action.

namespace chromeos_update_engine {

// The default "official" Omaha update URL.
extern const char* const kProductionOmahaUrl;

class SystemState;

// This class encapsulates the data Omaha gets for the request, along with
// essential state needed for the processing of the request/response.  The
// strings in this struct should not be XML escaped.
//
// TODO (jaysri): chromium-os:39752 tracks the need to rename this class to
// reflect its lifetime more appropriately.
class OmahaRequestParams {
 public:
  OmahaRequestParams(SystemState* system_state)
      : system_state_(system_state),
        os_platform_(kOsPlatform),
        os_version_(kOsVersion),
        app_id_(kAppId),
	app_channel_(kDefaultChannel),
        delta_okay_(true),
        interactive_(false) {}

  OmahaRequestParams(SystemState* system_state,
                     const std::string& in_os_platform,
                     const std::string& in_os_version,
                     const std::string& in_os_sp,
                     const std::string& in_os_board,
                     const std::string& in_app_id,
                     const std::string& in_app_version,
                     const std::string& in_app_lang,
                     const std::string& in_app_channel,
                     const std::string& in_hwid,
                     const std::string& in_bootid,
                     bool in_delta_okay,
                     bool in_interactive,
                     const std::string& in_update_url)
      : system_state_(system_state),
        os_platform_(in_os_platform),
        os_version_(in_os_version),
        os_sp_(in_os_sp),
        os_board_(in_os_board),
        app_id_(in_app_id),
        app_version_(in_app_version),
        app_lang_(in_app_lang),
        app_channel_(in_app_channel),
        hwid_(in_hwid),
        bootid_(in_bootid),
        delta_okay_(in_delta_okay),
        interactive_(in_interactive),
        update_url_(in_update_url) {}

  // Setters and getters for the various properties.
  inline std::string os_platform() const { return os_platform_; }
  inline std::string os_version() const { return os_version_; }
  inline std::string os_sp() const { return os_sp_; }
  inline std::string os_board() const { return os_board_; }
  inline std::string app_id() const { return app_id_; }
  inline std::string app_lang() const { return app_lang_; }
  inline std::string hwid() const { return hwid_; }
  inline std::string bootid() const { return bootid_; }
  inline std::string machineid() const { return machineid_; }
  inline std::string oemid() const { return oemid_; }
  inline std::string oemversion() const { return oemversion_; }
  inline std::string alephversion() const { return alephversion_; }

  inline void set_app_version(const std::string& version) {
    app_version_ = version;
  }
  inline std::string app_version() const { return app_version_; }
  inline std::string app_channel() const { return app_channel_; }

  // Can client accept a delta ?
  inline void set_delta_okay(bool ok) { delta_okay_ = ok; }
  inline bool delta_okay() const { return delta_okay_; }

  // True if this is a user-initiated update check.
  inline bool interactive() const { return interactive_; }

  inline void set_update_url(const std::string& url) { update_url_ = url; }
  inline std::string update_url() const { return update_url_; }

  // Suggested defaults
  static const char* const kAppId;
  static const char* const kOsPlatform;
  static const char* const kOsVersion;
  static const char* const kUpdateUrl;
  static const char* const kDefaultChannel;

  // Initializes all the data in the object.
  // Returns true on success, false otherwise.
  bool Init(bool interactive);

  // For unit-tests.
  void set_root(const std::string& root);

 private:
  // Fetches the value for a given key from update.conf files.
  std::string GetConfValue(const std::string& key,
                           const std::string& default_value) const;

  // Fetches the value for a given key from /etc/oem-release.
  std::string GetOemValue(const std::string& key,
                          const std::string& default_value) const;

  // Common implementation of GetConfValue and GetOemValue.
  std::string SearchConfValue(
      const std::vector<std::string>& files,
      const std::string& key,
      const std::string& default_value) const;

  // Gets the machine type (e.g. "i686").
  std::string GetMachineType() const;

  // Global system context.
  SystemState* system_state_;

  // Basic properties of the OS and Application that go into the Omaha request.
  std::string os_platform_;
  std::string os_version_;
  std::string os_sp_;
  std::string os_board_;

  std::string app_id_;	// The app_id identifies CoreOS to the update service.
  std::string app_version_;
  std::string app_lang_;
  std::string app_channel_; // Current update group, aka channel, aka track.
  std::string hwid_;  // Hardware Qualification ID of the client
  std::string bootid_;  // Kernel generated guid that identifies this boot
  std::string machineid_; // Unique machine ID that is set during installation 
  std::string oemid_; // Unique machine ID that is set during installation 
  std::string oemversion_; // CoreOS version set during installation
  std::string alephversion_; // first CoreOS version cached by update_engine
  bool delta_okay_;  // If this client can accept a delta
  bool interactive_;   // Whether this is a user-initiated update check

  // The URL to send the Omaha request to.
  std::string update_url_;

  // When reading files, prepend root_ to the paths. Useful for testing.
  std::string root_;

  // TODO(jaysri): Uncomment this after fixing unit tests, as part of
  // chromium-os:39752
  // DISALLOW_COPY_AND_ASSIGN(OmahaRequestParams);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_PARAMS_H__
