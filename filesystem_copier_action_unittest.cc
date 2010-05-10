// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glib.h>
#include <set>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "base/string_util.h"
#include "update_engine/filesystem_copier_action.h"
#include "update_engine/filesystem_iterator.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::set;
using std::string;
using std::vector;

namespace chromeos_update_engine {

class FilesystemCopierActionTest : public ::testing::Test {
 protected:
  void DoTest(bool run_out_of_space,
              bool terminate_early,
              bool use_kernel_partition);
  void SetUp() {
  }
  void TearDown() {
  }
};

class FilesystemCopierActionTestDelegate : public ActionProcessorDelegate {
 public:
  FilesystemCopierActionTestDelegate() : ran_(false), success_(false) {}
  void ExitMainLoop() {
    while (g_main_context_pending(NULL)) {
      g_main_context_iteration(NULL, false);
    }
    g_main_loop_quit(loop_);
  }
  void ProcessingDone(const ActionProcessor* processor, bool success) {
    ExitMainLoop();
  }
  void ProcessingStopped(const ActionProcessor* processor) {
    ExitMainLoop();
  }
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       bool success) {
    if (action->Type() == FilesystemCopierAction::StaticType()) {
      ran_ = true;
      success_ = success;
    }
  }
  void set_loop(GMainLoop* loop) {
    loop_ = loop;
  }
  bool ran() { return ran_; }
  bool success() { return success_; }
 private:
  GMainLoop* loop_;
  bool ran_;
  bool success_;
};

struct StartProcessorCallbackArgs {
  ActionProcessor* processor;
  FilesystemCopierAction* filesystem_copier_action;
  bool terminate_early;
};

gboolean StartProcessorInRunLoop(gpointer data) {
  StartProcessorCallbackArgs* args =
      reinterpret_cast<StartProcessorCallbackArgs*>(data);
  ActionProcessor* processor = args->processor;
  processor->StartProcessing();
  if (args->terminate_early) {
    EXPECT_TRUE(args->filesystem_copier_action);
    args->processor->StopProcessing();
  }
  return FALSE;
}

TEST_F(FilesystemCopierActionTest, RunAsRootSimpleTest) {
  ASSERT_EQ(0, getuid());
  DoTest(false, false, false);

  DoTest(false, false, true);
}
void FilesystemCopierActionTest::DoTest(bool run_out_of_space,
                                        bool terminate_early,
                                        bool use_kernel_partition) {
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);

  string a_loop_file;
  string b_loop_file;
  
  EXPECT_TRUE(utils::MakeTempFile("/tmp/a_loop_file.XXXXXX",
                                  &a_loop_file,
                                  NULL));
  ScopedPathUnlinker a_loop_file_unlinker(a_loop_file);
  EXPECT_TRUE(utils::MakeTempFile("/tmp/b_loop_file.XXXXXX",
                                  &b_loop_file,
                                  NULL));
  ScopedPathUnlinker b_loop_file_unlinker(b_loop_file);
  
  // Make random data for a, zero filled data for b.
  const size_t kLoopFileSize = 10 * 1024 * 1024 + 512;
  vector<char> a_loop_data(kLoopFileSize);
  FillWithData(&a_loop_data);
  vector<char> b_loop_data(run_out_of_space ?
                           (kLoopFileSize - 1) :
                           kLoopFileSize,
                           '\0');  // Fill with 0s

  // Write data to disk
  EXPECT_TRUE(WriteFileVector(a_loop_file, a_loop_data));
  EXPECT_TRUE(WriteFileVector(b_loop_file, b_loop_data));

  // Make loop devices for the files
  string a_dev = GetUnusedLoopDevice();
  EXPECT_FALSE(a_dev.empty());
  EXPECT_EQ(0, System(StringPrintf("losetup %s %s",
                                   a_dev.c_str(),
                                   a_loop_file.c_str())));
  ScopedLoopbackDeviceReleaser a_dev_releaser(a_dev);

  string b_dev = GetUnusedLoopDevice();
  EXPECT_FALSE(b_dev.empty());
  EXPECT_EQ(0, System(StringPrintf("losetup %s %s",
                                   b_dev.c_str(),
                                   b_loop_file.c_str())));
  ScopedLoopbackDeviceReleaser b_dev_releaser(b_dev);

  // Set up the action objects
  InstallPlan install_plan;
  install_plan.is_full_update = false;
  if (use_kernel_partition)
    install_plan.kernel_install_path = b_dev;
  else
    install_plan.install_path = b_dev;

  ActionProcessor processor;
  FilesystemCopierActionTestDelegate delegate;
  delegate.set_loop(loop);
  processor.set_delegate(&delegate);

  ObjectFeederAction<InstallPlan> feeder_action;
  FilesystemCopierAction copier_action(use_kernel_partition);
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&feeder_action, &copier_action);
  BondActions(&copier_action, &collector_action);

  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&copier_action);
  processor.EnqueueAction(&collector_action);

  copier_action.set_copy_source(a_dev);
  feeder_action.set_obj(install_plan);

  StartProcessorCallbackArgs start_callback_args;
  start_callback_args.processor = &processor;
  start_callback_args.filesystem_copier_action = &copier_action;
  start_callback_args.terminate_early = terminate_early;

  g_timeout_add(0, &StartProcessorInRunLoop, &start_callback_args);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);

  if (!terminate_early)
    EXPECT_TRUE(delegate.ran());
  if (run_out_of_space || terminate_early) {
    EXPECT_FALSE(delegate.success());
    return;
  }
  EXPECT_TRUE(delegate.success());

  // Make sure everything in the out_image is there
  vector<char> a_out;
  vector<char> b_out;
  EXPECT_TRUE(utils::ReadFile(a_dev, &a_out));
  EXPECT_TRUE(utils::ReadFile(b_dev, &b_out));
  EXPECT_TRUE(ExpectVectorsEq(a_out, b_out));
  EXPECT_TRUE(ExpectVectorsEq(a_loop_data, a_out));

  EXPECT_TRUE(collector_action.object() == install_plan);
}

class FilesystemCopierActionTest2Delegate : public ActionProcessorDelegate {
 public:
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       bool success) {
    if (action->Type() == FilesystemCopierAction::StaticType()) {
      ran_ = true;
      success_ = success;
    }
  }
  GMainLoop *loop_;
  bool ran_;
  bool success_;
};

TEST_F(FilesystemCopierActionTest, MissingInputObjectTest) {
  ActionProcessor processor;
  FilesystemCopierActionTest2Delegate delegate;

  processor.set_delegate(&delegate);

  FilesystemCopierAction copier_action(false);
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&copier_action, &collector_action);

  processor.EnqueueAction(&copier_action);
  processor.EnqueueAction(&collector_action);
  processor.StartProcessing();
  EXPECT_FALSE(processor.IsRunning());
  EXPECT_TRUE(delegate.ran_);
  EXPECT_FALSE(delegate.success_);
}

TEST_F(FilesystemCopierActionTest, FullUpdateTest) {
  ActionProcessor processor;
  FilesystemCopierActionTest2Delegate delegate;

  processor.set_delegate(&delegate);

  ObjectFeederAction<InstallPlan> feeder_action;
  const char* kUrl = "http://some/url";
  InstallPlan install_plan(true, kUrl, 0, "", "", "");
  feeder_action.set_obj(install_plan);
  FilesystemCopierAction copier_action(false);
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&feeder_action, &copier_action);
  BondActions(&copier_action, &collector_action);

  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&copier_action);
  processor.EnqueueAction(&collector_action);
  processor.StartProcessing();
  EXPECT_FALSE(processor.IsRunning());
  EXPECT_TRUE(delegate.ran_);
  EXPECT_TRUE(delegate.success_);
  EXPECT_EQ(kUrl, collector_action.object().download_url);
}

TEST_F(FilesystemCopierActionTest, NonExistentDriveTest) {
  ActionProcessor processor;
  FilesystemCopierActionTest2Delegate delegate;

  processor.set_delegate(&delegate);

  ObjectFeederAction<InstallPlan> feeder_action;
  InstallPlan install_plan(false, "", 0, "", "/no/such/file", "/no/such/file");
  feeder_action.set_obj(install_plan);
  FilesystemCopierAction copier_action(false);
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&copier_action, &collector_action);

  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&copier_action);
  processor.EnqueueAction(&collector_action);
  processor.StartProcessing();
  EXPECT_FALSE(processor.IsRunning());
  EXPECT_TRUE(delegate.ran_);
  EXPECT_FALSE(delegate.success_);
}

TEST_F(FilesystemCopierActionTest, RunAsRootNoSpaceTest) {
  ASSERT_EQ(0, getuid());
  DoTest(true, false, false);
}

TEST_F(FilesystemCopierActionTest, RunAsRootTerminateEarlyTest) {
  ASSERT_EQ(0, getuid());
  DoTest(false, true, false);
}

}  // namespace chromeos_update_engine
