// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/install_plan.h"

#include "base/logging.h"

#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

InstallPlan::InstallPlan(bool is_resume,
                         const string& url,
                         uint64_t payload_size,
                         const string& payload_hash,
                         const string& install_path,
                         const string& kernel_install_path)
    : is_resume(is_resume),
      download_url(url),
      payload_size(payload_size),
      payload_hash(payload_hash),
      install_path(install_path),
      kernel_install_path(kernel_install_path),
      kernel_size(0),
      rootfs_size(0),
      hash_checks_mandatory(false),
      powerwash_required(false) {}

InstallPlan::InstallPlan() : is_resume(false),
                             payload_size(0),
                             kernel_size(0),
                             rootfs_size(0),
                             hash_checks_mandatory(false),
                             powerwash_required(false) {}


bool InstallPlan::operator==(const InstallPlan& that) const {
  return ((is_resume == that.is_resume) &&
          (download_url == that.download_url) &&
          (payload_size == that.payload_size) &&
          (payload_hash == that.payload_hash) &&
          (install_path == that.install_path) &&
          (kernel_install_path == that.kernel_install_path));
}

bool InstallPlan::operator!=(const InstallPlan& that) const {
  return !((*this) == that);
}

void InstallPlan::Dump() const {
  LOG(INFO) << "InstallPlan: "
            << (is_resume ? ", resume" : ", new_update")
            << ", url: " << download_url
            << ", payload size: " << payload_size
            << ", payload hash: " << payload_hash
            << ", install_path: " << install_path
            << ", kernel_install_path: " << kernel_install_path
            << ", hash_checks_mandatory: " << utils::ToString(
                hash_checks_mandatory)
            << ", powerwash_required: " << utils::ToString(
                powerwash_required);
}

}  // namespace chromeos_update_engine

