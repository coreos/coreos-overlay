// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_INSTALL_PLAN_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_INSTALL_PLAN_H__

#include <string>
#include <vector>

// InstallPlan is a simple struct that contains relevant info for many
// parts of the update system about the install that should happen.
namespace chromeos_update_engine {

struct InstallPlan {
  InstallPlan(bool is_resume,
              const std::string& url,
              uint64_t payload_size,
              const std::string& payload_hash,
              const std::string& install_path);

  // Default constructor: Initialize all members which don't have a class
  // initializer.
  InstallPlan();

  bool operator==(const InstallPlan& that) const;
  bool operator!=(const InstallPlan& that) const;

  void Dump() const;

  bool is_resume;
  std::string download_url;  // url to download from

  uint64_t payload_size;                 // size of the payload
  std::string payload_hash ;             // SHA256 hash of the payload
  std::string install_path;              // path to install device

  // The fields below are used for rootfs verification. The flow is:
  //
  // 1. FilesystemCopierAction(verify_hash=false) computes and fills in the
  // source partition sizes and hashes.
  //
  // 2. DownloadAction verifies the source partition sizes and hashes against
  // the expected values transmitted in the update manifest. It fills in the
  // expected applied partition sizes and hashes based on the manifest.
  //
  // 4. FilesystemCopierAction(verify_hashes=true) computes and verifies the
  // applied partition sizes and hashes against the expected values.
  uint64_t rootfs_size;
  std::vector<char> rootfs_hash;

  // True if payload hash checks are mandatory based on the system state and
  // the Omaha response.
  bool hash_checks_mandatory;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_INSTALL_PLAN_H__
