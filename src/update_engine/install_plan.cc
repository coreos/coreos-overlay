// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/install_plan.h"

#include <glog/logging.h>

#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

InstallPlan::InstallPlan(bool is_resume,
                         const string& url,
                         uint64_t payload_size,
                         const string& payload_hash,
                         const string& partition_path)
    : is_resume(is_resume),
      download_url(url),
      payload_size(payload_size),
      payload_hash(payload_hash),
      partition_path(partition_path),
      kernel_path(utils::BootKernelName(partition_path)),
      new_partition_size(0) {}

InstallPlan::InstallPlan() : is_resume(false),
                             payload_size(0),
                             new_partition_size(0) {}


bool InstallPlan::operator==(const InstallPlan& that) const {
  return ((is_resume == that.is_resume) &&
          (download_url == that.download_url) &&
          (payload_size == that.payload_size) &&
          (payload_hash == that.payload_hash) &&
          (partition_path == that.partition_path) &&
          (kernel_path == that.kernel_path));
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
            << ", partition_path: " << partition_path
            << ", kernel_path: " << kernel_path;
}

}  // namespace chromeos_update_engine

