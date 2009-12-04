// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "update_engine/postinstall_runner_action.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

class PostinstallRunnerActionTest : public ::testing::Test {
 public:
  void DoTest(bool do_losetup, bool do_err_script);
};

class PostinstActionProcessorDelegate : public ActionProcessorDelegate {
 public:
  PostinstActionProcessorDelegate() : success_(false), success_set_(false) {}
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       bool success) {
    if (action->Type() == PostinstallRunnerAction::StaticType()) {
      success_ = success;
      success_set_ = true;
    }
  }
  bool success_;
  bool success_set_;
};

TEST_F(PostinstallRunnerActionTest, RunAsRootSimpleTest) {
  ASSERT_EQ(0, getuid());
  DoTest(true, false);
}

TEST_F(PostinstallRunnerActionTest, RunAsRootCantMountTest) {
  ASSERT_EQ(0, getuid());
  DoTest(false, false);
}

TEST_F(PostinstallRunnerActionTest, RunAsRootErrScriptTest) {
  ASSERT_EQ(0, getuid());
  DoTest(true, true);
}

void PostinstallRunnerActionTest::DoTest(bool do_losetup, bool do_err_script) {
  ASSERT_EQ(0, getuid()) << "Run me as root. Ideally don't run other tests "
                         << "as root, tho.";

  const string mountpoint(utils::kStatefulPartition + "/au_destination");

  string cwd;
  {
    vector<char> buf(1000);
    ASSERT_EQ(&buf[0], getcwd(&buf[0], buf.size()));
    cwd = string(&buf[0], strlen(&buf[0]));
  }

  // create the au destination, if it doesn't exist
  ASSERT_EQ(0, System(string("mkdir -p ") + mountpoint));

  // create 10MiB sparse file
  ASSERT_EQ(0, system("dd if=/dev/zero of=image.dat seek=10485759 bs=1 "
                      "count=1"));

  // format it as ext3
  ASSERT_EQ(0, system("mkfs.ext3 -F image.dat"));

  // mount it
  ASSERT_EQ(0, System(string("mount -o loop image.dat ") + mountpoint));

  // put a postinst script in
  string script = string("#!/bin/bash\ntouch ") + cwd + "/postinst_called\n";
  if (do_err_script) {
    script = "#!/bin/bash\nexit 1";
  }
  ASSERT_TRUE(WriteFileString(mountpoint + "/postinst", script));
  ASSERT_EQ(0, System(string("chmod a+x ") + mountpoint + "/postinst"));

  ASSERT_EQ(0, System(string("umount ") + mountpoint));

  ASSERT_EQ(0, System(string("rm -f ") + cwd + "/postinst_called"));

  // get a loop device we can use for the install device
  FILE* find_dev_cmd = popen("losetup -f", "r");
  ASSERT_TRUE(find_dev_cmd);

  char dev[100] = {0};
  size_t r = fread(dev, 1, sizeof(dev), find_dev_cmd);
  ASSERT_GT(r, 0);
  ASSERT_LT(r, sizeof(dev));
  ASSERT_TRUE(feof(find_dev_cmd));
  fclose(find_dev_cmd);

  // strip trailing newline on dev
  if (dev[strlen(dev) - 1] == '\n')
    dev[strlen(dev) - 1] = '\0';

  if (do_losetup)
    ASSERT_EQ(0, System(string("losetup ") + dev + " " + cwd + "/image.dat"));

  ActionProcessor processor;
  ObjectFeederAction<string> feeder_action;
  feeder_action.set_obj(dev);
  PostinstallRunnerAction runner_action;
  BondActions(&feeder_action, &runner_action);
  ObjectCollectorAction<string> collector_action;
  BondActions(&runner_action, &collector_action);
  PostinstActionProcessorDelegate delegate;
  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&runner_action);
  processor.EnqueueAction(&collector_action);
  processor.set_delegate(&delegate);
  processor.StartProcessing();
  ASSERT_FALSE(processor.IsRunning())
      << "Update test to handle non-asynch actions";

  EXPECT_TRUE(delegate.success_set_);
  EXPECT_EQ(do_losetup && !do_err_script, delegate.success_);
  EXPECT_EQ(do_losetup && !do_err_script, !collector_action.object().empty());
  if (do_losetup && !do_err_script) {
    EXPECT_EQ(dev, collector_action.object());
  }

  struct stat stbuf;
  int rc = lstat((string(cwd) + "/postinst_called").c_str(), &stbuf);
  if (do_losetup && !do_err_script)
    ASSERT_EQ(0, rc);
  else
    ASSERT_LT(rc, 0);

  if (do_losetup)
    ASSERT_EQ(0, System(string("losetup -d ") + dev));
  ASSERT_EQ(0, System(string("rm -f ") + cwd + "/postinst_called"));
  ASSERT_EQ(0, System(string("rm -f ") + cwd + "/image.dat"));
}

// Death tests don't seem to be working on Hardy
TEST_F(PostinstallRunnerActionTest, DISABLED_RunAsRootDeathTest) {
  ASSERT_EQ(0, getuid());
  PostinstallRunnerAction runner_action;
  ASSERT_DEATH({ runner_action.TerminateProcessing(); },
               "postinstall_runner_action.h:.*] Check failed");
}

}  // namespace chromeos_update_engine
