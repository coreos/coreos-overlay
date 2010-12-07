// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
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
  InstallPlan(bool is_full,
              bool is_resume,
              const std::string& url,
              uint64_t size,
              const std::string& hash,
              const std::string& install_path,
              const std::string& kernel_install_path)
      : is_full_update(is_full),
        is_resume(is_resume),
        download_url(url),
        size(size),
        download_hash(hash),
        install_path(install_path),
        kernel_install_path(kernel_install_path),
        kernel_size(0),
        rootfs_size(0) {}
  InstallPlan() : is_full_update(false), is_resume(false), size(0) {}

  bool is_full_update;
  bool is_resume;
  std::string download_url;  // url to download from
  uint64_t size;  // size of the download url's data
  std::string download_hash;  // hash of the data at the url
  std::string install_path;  // path to install device
  std::string kernel_install_path;  // path to kernel install device

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
    return (is_full_update == that.is_full_update) &&
        (is_resume == that.is_resume) &&
        (download_url == that.download_url) &&
        (size == that.size) &&
        (download_hash == that.download_hash) &&
        (install_path == that.install_path) &&
        (kernel_install_path == that.kernel_install_path);
  }
  bool operator!=(const InstallPlan& that) const {
    return !((*this) == that);
  }
  void Dump() const {
    LOG(INFO) << "InstallPlan: "
              << (is_full_update ? "full_update" : "delta_update")
              << (is_resume ? ", resume" : ", new_update")
              << ", url: " << download_url
              << ", size: " << size
              << ", hash: " << download_hash
              << ", install_path: " << install_path
              << ", kernel_install_path: " << kernel_install_path;
  }
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_INSTALL_PLAN_H__
