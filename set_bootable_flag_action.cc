// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/set_bootable_flag_action.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include "update_engine/subprocess.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
const char* const kGpt = "/usr/bin/gpt";
const ssize_t kPmbrLength = 512;
}

void SetBootableFlagAction::PerformAction() {
  ScopedActionCompleter completer(processor_, this);

  TEST_AND_RETURN(HasInputObject());
  const InstallPlan install_plan = GetInputObject();
  TEST_AND_RETURN(!install_plan.install_path.empty());
  const string partition = install_plan.install_path;
  string root_device = utils::RootDevice(partition);
  string partition_number = utils::PartitionNumber(partition);

  utils::BootLoader boot_loader;
  TEST_AND_RETURN(utils::GetBootloader(&boot_loader));

  // For now, only support Syslinux bootloader
  TEST_AND_RETURN(boot_loader == utils::BootLoader_SYSLINUX);

  string temp_file;
  TEST_AND_RETURN(utils::MakeTempFile("/tmp/pmbr_copy.XXXXXX",
                                      &temp_file,
                                      NULL));
  ScopedPathUnlinker temp_file_unlinker(temp_file);

  // Copy existing PMBR to temp file
  vector<char> buf(kPmbrLength);
  {
    int fd = open(root_device.c_str(), O_RDONLY);
    TEST_AND_RETURN(fd >= 0);
    ScopedFdCloser fd_closer(&fd);
    ssize_t bytes_read;
    TEST_AND_RETURN(utils::PReadAll(fd, &buf[0], buf.size(), 0, &bytes_read));
  }
  TEST_AND_RETURN(utils::WriteFile(temp_file.c_str(), &buf[0], buf.size()));

  // Call gpt tool to do the work
  vector<string> command;
  command.push_back(kGpt);
  command.push_back("-S");
  command.push_back("boot");
  command.push_back("-i");
  command.push_back(partition_number);
  command.push_back("-b");
  command.push_back(temp_file);
  command.push_back(root_device);
  int rc = 0;
  Subprocess::SynchronousExec(command, &rc);
  TEST_AND_RETURN(rc == 0);

  completer.set_success(true);
  if (HasOutputPipe())
    SetOutputObject(GetInputObject());
}

}  // namespace chromeos_update_engine
