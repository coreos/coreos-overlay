// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/postinstall_runner_action.h"
#include <sys/mount.h>
#include <stdlib.h>
#include "update_engine/utils.h"

namespace chromeos_update_engine {

using std::string;

namespace {
const string kMountPath(string(utils::kStatefulPartition) + "/au_destination");
const string kPostinstallScript("/postinst");
}

void PostinstallRunnerAction::PerformAction() {
  CHECK(HasInputObject());
  const string install_device = GetInputObject();

  int rc = mount(install_device.c_str(), kMountPath.c_str(), "ext3", 0, NULL);
  if (rc < 0) {
    LOG(ERROR) << "Unable to mount destination device " << install_device
               << " onto " << kMountPath;
    processor_->ActionComplete(this, false);
    return;
  }

  // run postinstall script
  rc = system((kMountPath + kPostinstallScript + " " + install_device).c_str());
  bool success = (rc == 0);
  if (!success) {
    LOG(ERROR) << "Postinst command failed with code: " << rc;
  }

  rc = umount(kMountPath.c_str());
  if (rc < 0) {
    // non-fatal
    LOG(ERROR) << "Unable to umount destination device";
  }
  if (success && HasOutputPipe()) {
    SetOutputObject(install_device);
  }
  processor_->ActionComplete(this, success);
}

}  // namespace chromeos_update_engine
