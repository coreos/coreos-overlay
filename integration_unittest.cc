// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>
#include <glib.h>
#include <pthread.h>
#include <gtest/gtest.h>
#include "update_engine/download_action.h"
#include "update_engine/install_action.h"
#include "update_engine/libcurl_http_fetcher.h"
#include "update_engine/mock_http_fetcher.h"
#include "update_engine/omaha_request_prep_action.h"
#include "update_engine/omaha_response_handler_action.h"
#include "update_engine/postinstall_runner_action.h"
#include "update_engine/set_bootable_flag_action.h"
#include "update_engine/test_utils.h"
#include "update_engine/update_check_action.h"
#include "update_engine/utils.h"

// The tests here integrate many Action objects together. This test that
// the objects work well together, whereas most other tests focus on a single
// action at a time.

namespace chromeos_update_engine {

using std::string;
using std::vector;

class IntegrationTest : public ::testing::Test { };

namespace {
const char* kTestDir = "/tmp/update_engine-integration-test";

class IntegrationTestProcessorDelegate : public ActionProcessorDelegate {
 public:
  IntegrationTestProcessorDelegate()
      : loop_(NULL), processing_done_called_(false) {}
  virtual ~IntegrationTestProcessorDelegate() {
    EXPECT_TRUE(processing_done_called_);
  }
  virtual void ProcessingDone(const ActionProcessor* processor) {
    processing_done_called_ = true;
    g_main_loop_quit(loop_);
  }

  virtual void ActionCompleted(ActionProcessor* processor,
                               AbstractAction* action,
                               bool success) {
    // make sure actions always succeed
    EXPECT_TRUE(success);

    // Swap in the device path for PostinstallRunnerAction with a loop device
    if (action->Type() == InstallAction::StaticType()) {
      InstallAction* install_action = static_cast<InstallAction*>(action);
      old_dev_ = install_action->GetOutputObject();
      string dev = GetUnusedLoopDevice();
      string cmd = string("losetup ") + dev + " " + kTestDir + "/dev2";
      EXPECT_EQ(0, system(cmd.c_str()));
      install_action->SetOutputObject(dev);
    } else if (action->Type() == PostinstallRunnerAction::StaticType()) {
      PostinstallRunnerAction* postinstall_runner_action =
          static_cast<PostinstallRunnerAction*>(action);
      string dev = postinstall_runner_action->GetOutputObject();
      EXPECT_EQ(0, system((string("losetup -d ") + dev).c_str()));
      postinstall_runner_action->SetOutputObject(old_dev_);
      old_dev_ = "";
    }
  }

  void set_loop(GMainLoop* loop) {
    loop_ = loop;
  }

 private:
  GMainLoop *loop_;
  bool processing_done_called_;

  // We have to change the dev for the PostinstallRunnerAction action.
  // Before that runs, we store the device here, and after it runs, we
  // restore it.
  // This is because we use a file, rather than a device, to install into,
  // but the PostinstallRunnerAction requires a real device. We set up a
  // loop device pointing to the file when necessary.
  string old_dev_;
};

gboolean TestStarter(gpointer data) {
  ActionProcessor *processor = reinterpret_cast<ActionProcessor*>(data);
  processor->StartProcessing();
  return FALSE;
}

}  // namespace {}

TEST(IntegrationTest, DISABLED_RunAsRootFullInstallTest) {
  ASSERT_EQ(0, getuid());
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);

  ActionProcessor processor;
  IntegrationTestProcessorDelegate delegate;
  delegate.set_loop(loop);
  processor.set_delegate(&delegate);

  // Actions:
  OmahaRequestPrepAction request_prep_action(false);
  UpdateCheckAction update_check_action(new LibcurlHttpFetcher);
  OmahaResponseHandlerAction response_handler_action;
  DownloadAction download_action(new LibcurlHttpFetcher);
  InstallAction install_action;
  PostinstallRunnerAction postinstall_runner_action;
  SetBootableFlagAction set_bootable_flag_action;

  // Enqueue the actions
  processor.EnqueueAction(&request_prep_action);
  processor.EnqueueAction(&update_check_action);
  processor.EnqueueAction(&response_handler_action);
  processor.EnqueueAction(&download_action);
  processor.EnqueueAction(&install_action);
  processor.EnqueueAction(&postinstall_runner_action);
  processor.EnqueueAction(&set_bootable_flag_action);

  // Bond them together
  BondActions(&request_prep_action, &update_check_action);
  BondActions(&update_check_action, &response_handler_action);
  BondActions(&response_handler_action, &download_action);
  BondActions(&download_action, &install_action);
  BondActions(&install_action, &postinstall_runner_action);
  BondActions(&postinstall_runner_action, &set_bootable_flag_action);

  // Set up filesystem to trick some of the actions
  ASSERT_EQ(0, System(string("rm -rf ") + kTestDir));
  ASSERT_EQ(0, system("rm -f /tmp/update_engine_test_postinst_out.txt"));
  ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir + "/etc"));
  ASSERT_TRUE(WriteFileString(string(kTestDir) + "/etc/lsb-release",
                              "GOOGLE_RELEASE=0.2.0.0\n"
                              "GOOGLE_TRACK=unittest-track"));
  ASSERT_EQ(0, System(string("touch ") + kTestDir + "/dev1"));
  ASSERT_EQ(0, System(string("touch ") + kTestDir + "/dev2"));
  ASSERT_TRUE(WriteFileVector(string(kTestDir) + "/dev", GenerateSampleMbr()));

  request_prep_action.set_root(kTestDir);
  response_handler_action.set_boot_device(string(kTestDir) + "/dev1");

  // Run the actions
  g_timeout_add(0, &TestStarter, &processor);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);

  // Verify the results
  struct stat stbuf;
  ASSERT_EQ(0, lstat((string(kTestDir) + "/dev1").c_str(), &stbuf));
  EXPECT_EQ(0, stbuf.st_size);
  EXPECT_TRUE(S_ISREG(stbuf.st_mode));
  ASSERT_EQ(0, lstat((string(kTestDir) + "/dev2").c_str(), &stbuf));
  EXPECT_EQ(996147200, stbuf.st_size);
  EXPECT_TRUE(S_ISREG(stbuf.st_mode));
  ASSERT_EQ(0, lstat((string(kTestDir) + "/dev").c_str(), &stbuf));
  ASSERT_EQ(512, stbuf.st_size);
  EXPECT_TRUE(S_ISREG(stbuf.st_mode));
  vector<char> new_mbr;
  EXPECT_TRUE(utils::ReadFile((string(kTestDir) + "/dev").c_str(), &new_mbr));

  // Check bootable flag in MBR
  for (int i = 0; i < 4; i++) {
    char expected_flag = '\0';
    if (i == 1)
      expected_flag = 0x80;
    EXPECT_EQ(expected_flag, new_mbr[446 + 16 * i]);
  }
  // Check MBR signature
  EXPECT_EQ(static_cast<char>(0x55), new_mbr[510]);
  EXPECT_EQ(static_cast<char>(0xaa), new_mbr[511]);

  ASSERT_EQ(0, lstat("/tmp/update_engine_test_postinst_out.txt", &stbuf));
  EXPECT_TRUE(S_ISREG(stbuf.st_mode));
  string file_data;
  EXPECT_TRUE(utils::ReadFileToString(
      "/tmp/update_engine_test_postinst_out.txt",
      &file_data));
  EXPECT_EQ("POSTINST_DONE\n", file_data);

  // cleanup
  ASSERT_EQ(0, System(string("rm -rf ") + kTestDir));
  ASSERT_EQ(0, system("rm -f /tmp/update_engine_test_postinst_out.txt"));
}

}  // namespace chromeos_update_engine
