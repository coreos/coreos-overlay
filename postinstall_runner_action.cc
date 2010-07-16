// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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
const string kPostinstallScript("/postinst");
}

void PostinstallRunnerAction::PerformAction() {
  CHECK(HasInputObject());
  const InstallPlan install_plan = GetInputObject();
  const string install_device = install_plan.install_path;
  ScopedActionCompleter completer(processor_, this);
  
  // Make mountpoint
  string temp_dir;
  TEST_AND_RETURN(utils::MakeTempDirectory("/tmp/au_postint_mount.XXXXXX",
                                           &temp_dir));
  ScopedDirRemover temp_dir_remover(temp_dir);

  {
    // Scope for the mount
    unsigned long mountflags = MS_RDONLY;

    int rc = mount(install_device.c_str(),
                   temp_dir.c_str(),
                   "ext4",
                   mountflags,
                   NULL);
    if (rc < 0) {
      LOG(INFO) << "Failed to mount install part as ext4. Trying ext3.";
      rc = mount(install_device.c_str(),
                 temp_dir.c_str(),
                 "ext3",
                 mountflags,
                 NULL);
    }
    if (rc < 0) {
      LOG(ERROR) << "Unable to mount destination device " << install_device
                 << " onto " << temp_dir;
      return;
    }
    ScopedFilesystemUnmounter unmounter(temp_dir);

    // run postinstall script
    vector<string> command;
    command.push_back(temp_dir + kPostinstallScript);
    command.push_back(install_device);
    command.push_back(precommit_ ? "" : "--postcommit");
    rc = 0;
    TEST_AND_RETURN(Subprocess::SynchronousExec(command, &rc));
    bool success = (rc == 0);
    if (!success) {
      LOG(ERROR) << "Postinst command failed with code: " << rc;
      return;
    }
  }

  if (HasOutputPipe()) {
    SetOutputObject(install_plan);
  }
  completer.set_success(true);
}

}  // namespace chromeos_update_engine
