// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_INSTALL_PLAN_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_INSTALL_PLAN_H__

#include <string>
#include <vector>

#include "base/logging.h"

// InstallPlan is a simple struct that contains relevant info for many
// parts of the update system about the install that should happen.

namespace chromeos_update_engine {

struct InstallPlan {
  InstallPlan(bool is_resume,
              const std::string& url,
              uint64_t payload_size,
              const std::string& payload_hash,
              uint64_t metadata_size,
              const std::string& metadata_signature,
              const std::string& install_path,
              const std::string& kernel_install_path)
      : is_resume(is_resume),
        download_url(url),
        payload_size(payload_size),
        payload_hash(payload_hash),
        metadata_size(metadata_size),
        metadata_signature(metadata_signature),
        install_path(install_path),
        kernel_install_path(kernel_install_path),
        kernel_size(0),
        rootfs_size(0) {}
  InstallPlan() : is_resume(false), payload_size(0), metadata_size(0) {}

  bool is_resume;
  std::string download_url;  // url to download from

  uint64_t payload_size;                 // size of the payload
  std::string payload_hash ;             // SHA256 hash of the payload
  uint64_t metadata_size;                // size of the metadata
  std::string metadata_signature;        // signature of the  metadata
  std::string install_path;              // path to install device
  std::string kernel_install_path;       // path to kernel install device

  // The fields below are used for kernel and rootfs verification. The flow is:
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
  uint64_t kernel_size;
  uint64_t rootfs_size;
  std::vector<char> kernel_hash;
  std::vector<char> rootfs_hash;

  bool operator==(const InstallPlan& that) const {
    return ((is_resume == that.is_resume) &&
            (download_url == that.download_url) &&
            (payload_size == that.payload_size) &&
            (payload_hash == that.payload_hash) &&
            (metadata_size == that.metadata_size) &&
            (metadata_signature == that.metadata_signature) &&
            (install_path == that.install_path) &&
            (kernel_install_path == that.kernel_install_path));
  }
  bool operator!=(const InstallPlan& that) const {
    return !((*this) == that);
  }
  void Dump() const {
    LOG(INFO) << "InstallPlan: "
              << (is_resume ? ", resume" : ", new_update")
              << ", url: " << download_url
              << ", payload size: " << payload_size
              << ", payload hash: " << payload_hash
              << ", metadata size: " << metadata_size
              << ", metadata signature: " << metadata_signature
              << ", install_path: " << install_path
              << ", kernel_install_path: " << kernel_install_path;
  }
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_INSTALL_PLAN_H__
