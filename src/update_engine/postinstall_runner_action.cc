// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/postinstall_runner_action.h"
#include <sys/mount.h>
#include <stdlib.h>
#include <vector>
#include "update_engine/subprocess.h"
#include "update_engine/utils.h"

namespace chromeos_update_engine {

using std::string;
using std::vector;

namespace {
const char kPostinstallScript[] = "/postinst";
}

void PostinstallRunnerAction::PerformAction() {
  CHECK(HasInputObject());
  const InstallPlan install_plan = GetInputObject();
  const string install_device = install_plan.partition_path;
  ScopedActionCompleter completer(processor_, this);

  // Make mountpoint.
  TEST_AND_RETURN(utils::MakeTempDirectory("/tmp/au_postint_mount.XXXXXX",
                                           &temp_rootfs_dir_));
  ScopedDirRemover temp_dir_remover(temp_rootfs_dir_);

  unsigned long mountflags = MS_RDONLY;
  int rc = mount(install_device.c_str(),
                 temp_rootfs_dir_.c_str(),
                 "ext2",
                 mountflags,
                 NULL);
  if (rc < 0) {
    LOG(INFO) << "Failed to mount install part as ext2. Trying ext3.";
    rc = mount(install_device.c_str(),
               temp_rootfs_dir_.c_str(),
               "ext3",
               mountflags,
               NULL);
  }
  if (rc < 0) {
    LOG(ERROR) << "Unable to mount destination device " << install_device
               << " onto " << temp_rootfs_dir_;
    return;
  }

  temp_dir_remover.set_should_remove(false);
  completer.set_should_complete(false);

  // Runs the postinstall script asynchronously to free up the main loop while
  // it's running.
  vector<string> command;
  command.push_back(temp_rootfs_dir_ + kPostinstallScript);
  command.push_back(install_device);
  if (!Subprocess::Get().Exec(command, StaticCompletePostinstall, this)) {
    CompletePostinstall(1);
  }
}

void PostinstallRunnerAction::CompletePostinstall(int return_code) {
  ScopedActionCompleter completer(processor_, this);
  ScopedTempUnmounter temp_unmounter(temp_rootfs_dir_);
  if (return_code != 0) {
    LOG(ERROR) << "Postinst command failed with code: " << return_code;
    if (return_code == 3) {
      // This special return code means that we tried to update firmware,
      // but couldn't because we booted from FW B, and we need to reboot
      // to get back to FW A.
      completer.set_code(kActionCodePostinstallBootedFromFirmwareB);
    }
    return;
  }

  LOG(INFO) << "Postinst command succeeded";
  CHECK(HasInputObject());
  const InstallPlan install_plan = GetInputObject();

  if (HasOutputPipe())
    SetOutputObject(install_plan);

  completer.set_code(kActionCodeSuccess);
}

void PostinstallRunnerAction::StaticCompletePostinstall(int return_code,
                                                        const string& output,
                                                        void* p) {
  reinterpret_cast<PostinstallRunnerAction*>(p)->CompletePostinstall(
      return_code);
}

}  // namespace chromeos_update_engine
