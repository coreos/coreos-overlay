// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>

#include <set>
#include <string>
#include <vector>

#include <base/eintr_wrapper.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <glib.h>
#include <gtest/gtest.h>

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
  // |verify_hash|: 0 - no hash verification, 1 -- successful hash verification,
  // 2 -- hash verification failure.
  // Returns true iff test has completed successfully.
  bool DoTest(bool run_out_of_space,
              bool terminate_early,
              bool use_kernel_partition,
              int verify_hash);
  void SetUp() {
  }
  void TearDown() {
  }
};

class FilesystemCopierActionTestDelegate : public ActionProcessorDelegate {
 public:
  FilesystemCopierActionTestDelegate() : ran_(false), code_(kActionCodeError) {}
  void ExitMainLoop() {
    while (g_main_context_pending(NULL)) {
      g_main_context_iteration(NULL, false);
    }
    g_main_loop_quit(loop_);
  }
  void ProcessingDone(const ActionProcessor* processor, ActionExitCode code) {
    ExitMainLoop();
  }
  void ProcessingStopped(const ActionProcessor* processor) {
    ExitMainLoop();
  }
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       ActionExitCode code) {
    if (action->Type() == FilesystemCopierAction::StaticType()) {
      ran_ = true;
      code_ = code;
    }
  }
  void set_loop(GMainLoop* loop) {
    loop_ = loop;
  }
  bool ran() { return ran_; }
  ActionExitCode code() { return code_; }
 private:
  GMainLoop* loop_;
  bool ran_;
  ActionExitCode code_;
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

// TODO(garnold) Temporarily disabling this test, see chromium-os:31082 for
// details; still trying to track down the root cause for these rare write
// failures and whether or not they are due to the test setup or an inherent
// issue with the chroot environiment, library versions we use, etc.
TEST_F(FilesystemCopierActionTest, DISABLED_RunAsRootSimpleTest) {
  ASSERT_EQ(0, getuid());
  bool test = DoTest(false, false, false, 0);
  EXPECT_TRUE(test);
  if (!test)
    return;
  test = DoTest(false, false, true, 0);
  EXPECT_TRUE(test);
}

bool FilesystemCopierActionTest::DoTest(bool run_out_of_space,
                                        bool terminate_early,
                                        bool use_kernel_partition,
                                        int verify_hash) {
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);

  string a_loop_file;
  string b_loop_file;

  if (!(utils::MakeTempFile("/tmp/a_loop_file.XXXXXX", &a_loop_file, NULL) &&
        utils::MakeTempFile("/tmp/b_loop_file.XXXXXX", &b_loop_file, NULL))) {
    ADD_FAILURE();
    return false;
  }
  ScopedPathUnlinker a_loop_file_unlinker(a_loop_file);
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
  if (!(WriteFileVector(a_loop_file, a_loop_data) &&
        WriteFileVector(b_loop_file, b_loop_data))) {
    ADD_FAILURE();
    return false;
  }

  // Attach loop devices to the files
  string a_dev;
  string b_dev;

  ScopedLoopbackDeviceBinder a_dev_releaser(a_loop_file, &a_dev);
  ScopedLoopbackDeviceBinder b_dev_releaser(b_loop_file, &b_dev);
  if (!(a_dev_releaser.is_bound() && b_dev_releaser.is_bound())) {
    ADD_FAILURE();
    return false;
  }

  LOG(INFO) << "copying: "
            << a_loop_file << " (" << a_dev << ") -> "
            << b_loop_file << " (" << b_dev << ", "
            << kLoopFileSize << " bytes";
  bool success = true;

  // Set up the action objects
  InstallPlan install_plan;
  if (verify_hash) {
    if (use_kernel_partition) {
      install_plan.kernel_install_path = a_dev;
      install_plan.kernel_size =
          kLoopFileSize - ((verify_hash == 2) ? 1 : 0);
      if (!OmahaHashCalculator::RawHashOfData(a_loop_data,
                                              &install_plan.kernel_hash)) {
        ADD_FAILURE();
        success = false;
      }
    } else {
      install_plan.install_path = a_dev;
      install_plan.rootfs_size =
          kLoopFileSize - ((verify_hash == 2) ? 1 : 0);
      if (!OmahaHashCalculator::RawHashOfData(a_loop_data,
                                              &install_plan.rootfs_hash)) {
        ADD_FAILURE();
        success = false;
      }
    }
  } else {
    if (use_kernel_partition) {
      install_plan.kernel_install_path = b_dev;
    } else {
      install_plan.install_path = b_dev;
    }
  }

  ActionProcessor processor;
  FilesystemCopierActionTestDelegate delegate;
  delegate.set_loop(loop);
  processor.set_delegate(&delegate);

  ObjectFeederAction<InstallPlan> feeder_action;
  FilesystemCopierAction copier_action(use_kernel_partition,
                                       verify_hash != 0);
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&feeder_action, &copier_action);
  BondActions(&copier_action, &collector_action);

  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&copier_action);
  processor.EnqueueAction(&collector_action);

  if (!verify_hash) {
    copier_action.set_copy_source(a_dev);
  }
  feeder_action.set_obj(install_plan);

  StartProcessorCallbackArgs start_callback_args;
  start_callback_args.processor = &processor;
  start_callback_args.filesystem_copier_action = &copier_action;
  start_callback_args.terminate_early = terminate_early;

  g_timeout_add(0, &StartProcessorInRunLoop, &start_callback_args);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);

  if (!terminate_early) {
    bool is_delegate_ran = delegate.ran();
    EXPECT_TRUE(is_delegate_ran);
    success = success && is_delegate_ran;
  }
  if (run_out_of_space || terminate_early) {
    EXPECT_EQ(kActionCodeError, delegate.code());
    return (kActionCodeError == delegate.code());
  }
  if (verify_hash == 2) {
    ActionExitCode expected_exit_code =
        (use_kernel_partition ?
         kActionCodeNewKernelVerificationError :
         kActionCodeNewRootfsVerificationError);
    EXPECT_EQ(expected_exit_code, delegate.code());
    return (expected_exit_code == delegate.code());
  }
  EXPECT_EQ(kActionCodeSuccess, delegate.code());

  // Make sure everything in the out_image is there
  vector<char> a_out;
  if (!utils::ReadFile(a_dev, &a_out)) {
    ADD_FAILURE();
    return false;
  }
  const bool is_a_file_reading_eq = ExpectVectorsEq(a_loop_data, a_out);
  EXPECT_TRUE(is_a_file_reading_eq);
  success = success && is_a_file_reading_eq;
  if (!verify_hash) {
    vector<char> b_out;
    if (!utils::ReadFile(b_dev, &b_out)) {
      ADD_FAILURE();
      return false;
    }
    const bool is_b_file_reading_eq = ExpectVectorsEq(a_out, b_out);
    EXPECT_TRUE(is_b_file_reading_eq);
    success = success && is_b_file_reading_eq;
  }

  bool is_install_plan_eq = (collector_action.object() == install_plan);
  EXPECT_TRUE(is_install_plan_eq);
  success = success && is_install_plan_eq;

  return success;
}

class FilesystemCopierActionTest2Delegate : public ActionProcessorDelegate {
 public:
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       ActionExitCode code) {
    if (action->Type() == FilesystemCopierAction::StaticType()) {
      ran_ = true;
      code_ = code;
    }
  }
  GMainLoop *loop_;
  bool ran_;
  ActionExitCode code_;
};

TEST_F(FilesystemCopierActionTest, MissingInputObjectTest) {
  ActionProcessor processor;
  FilesystemCopierActionTest2Delegate delegate;

  processor.set_delegate(&delegate);

  FilesystemCopierAction copier_action(false, false);
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&copier_action, &collector_action);

  processor.EnqueueAction(&copier_action);
  processor.EnqueueAction(&collector_action);
  processor.StartProcessing();
  EXPECT_FALSE(processor.IsRunning());
  EXPECT_TRUE(delegate.ran_);
  EXPECT_EQ(kActionCodeError, delegate.code_);
}

TEST_F(FilesystemCopierActionTest, ResumeTest) {
  ActionProcessor processor;
  FilesystemCopierActionTest2Delegate delegate;

  processor.set_delegate(&delegate);

  ObjectFeederAction<InstallPlan> feeder_action;
  const char* kUrl = "http://some/url";
  InstallPlan install_plan(true, kUrl, 0, "", 0, "", "", "");
  feeder_action.set_obj(install_plan);
  FilesystemCopierAction copier_action(false, false);
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&feeder_action, &copier_action);
  BondActions(&copier_action, &collector_action);

  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&copier_action);
  processor.EnqueueAction(&collector_action);
  processor.StartProcessing();
  EXPECT_FALSE(processor.IsRunning());
  EXPECT_TRUE(delegate.ran_);
  EXPECT_EQ(kActionCodeSuccess, delegate.code_);
  EXPECT_EQ(kUrl, collector_action.object().download_url);
}

TEST_F(FilesystemCopierActionTest, NonExistentDriveTest) {
  ActionProcessor processor;
  FilesystemCopierActionTest2Delegate delegate;

  processor.set_delegate(&delegate);

  ObjectFeederAction<InstallPlan> feeder_action;
  InstallPlan install_plan(false,
                           "",
                           0,
                           "",
                           0,
                           "",
                           "/no/such/file",
                           "/no/such/file");
  feeder_action.set_obj(install_plan);
  FilesystemCopierAction copier_action(false, false);
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&copier_action, &collector_action);

  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&copier_action);
  processor.EnqueueAction(&collector_action);
  processor.StartProcessing();
  EXPECT_FALSE(processor.IsRunning());
  EXPECT_TRUE(delegate.ran_);
  EXPECT_EQ(kActionCodeError, delegate.code_);
}

TEST_F(FilesystemCopierActionTest, RunAsRootVerifyHashTest) {
  ASSERT_EQ(0, getuid());
  EXPECT_TRUE(DoTest(false, false, false, 1));
  EXPECT_TRUE(DoTest(false, false, true, 1));
}

TEST_F(FilesystemCopierActionTest, RunAsRootVerifyHashFailTest) {
  ASSERT_EQ(0, getuid());
  EXPECT_TRUE(DoTest(false, false, false, 2));
  EXPECT_TRUE(DoTest(false, false, true, 2));
}

TEST_F(FilesystemCopierActionTest, RunAsRootNoSpaceTest) {
  ASSERT_EQ(0, getuid());
  EXPECT_TRUE(DoTest(true, false, false, 0));
}

TEST_F(FilesystemCopierActionTest, RunAsRootTerminateEarlyTest) {
  ASSERT_EQ(0, getuid());
  EXPECT_TRUE(DoTest(false, true, false, 0));
}

TEST_F(FilesystemCopierActionTest, RunAsRootDetermineFilesystemSizeTest) {
  string img;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/img.XXXXXX", &img, NULL));
  ScopedPathUnlinker img_unlinker(img);
  CreateExtImageAtPath(img, NULL);
  // Extend the "partition" holding the file system from 10MiB to 20MiB.
  EXPECT_EQ(0, System(StringPrintf(
      "dd if=/dev/zero of=%s seek=20971519 bs=1 count=1",
      img.c_str())));
  EXPECT_EQ(20 * 1024 * 1024, utils::FileSize(img));

  for (int i = 0; i < 2; ++i) {
    bool is_kernel = i == 1;
    FilesystemCopierAction action(is_kernel, false);
    EXPECT_EQ(kint64max, action.filesystem_size_);
    {
      int fd = HANDLE_EINTR(open(img.c_str(), O_RDONLY));
      EXPECT_TRUE(fd > 0);
      ScopedFdCloser fd_closer(&fd);
      action.DetermineFilesystemSize(fd);
    }
    EXPECT_EQ(is_kernel ? kint64max : 10 * 1024 * 1024,
              action.filesystem_size_);
  }
}


}  // namespace chromeos_update_engine
