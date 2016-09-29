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
              const std::string& partition_path);

  // Default constructor: Initialize all members which don't have a class
  // initializer.
  InstallPlan();

  bool operator==(const InstallPlan& that) const;
  bool operator!=(const InstallPlan& that) const;

  void Dump() const;

  bool is_resume;
  std::string download_url;  // url to download from

  uint64_t payload_size;                 // size of the payload
  std::string payload_hash;              // SHA256 hash of the payload
  std::string partition_path;            // path to main partition device
  std::string kernel_path;               // path to kernel image
  std::string pcr_policy_path;           // path to pcr policy zip file

  // Location to copy currently running system from.
  std::string old_partition_path;
  std::string old_kernel_path;

  // For verifying system state prior to applying the update. The partition
  // hash is computed by FilesystemCopierAction(verify_hash=false) and
  // later validated by PayloadProcessor once it receives the manifest.
  std::vector<char> old_partition_hash;
  std::vector<char> old_kernel_hash;

  // For verifying the update applied successfully. Values filled in by
  // PayloadProcessor once the update payload has been verified.
  // FilesystemCopierAction(verify_hashes=true) computes and verifies the
  // partition size and hash.
  uint64_t new_partition_size;
  std::vector<char> new_partition_hash;
  uint64_t new_kernel_size;
  std::vector<char> new_kernel_hash;
  uint64_t new_pcr_policy_size;
  std::vector<char> new_pcr_policy_hash;

  // Optional arguments to pass to the post install script. Used to inform
  // the script about optional update procedures that ran. For example
  // "KERNEL=kernel_path" indicates that update_engine installed the kernel
  // so the post install script should not do so.
  std::vector<std::string> postinst_args;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_INSTALL_PLAN_H__
