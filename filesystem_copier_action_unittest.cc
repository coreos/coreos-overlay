// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glib.h>
#include <set>
#include <string>
#include <vector>
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
  void DoTest(bool double_copy, bool run_out_of_space);
  string TestDir() { return "./FilesystemCopierActionTestDir"; }
  void SetUp() {
    System(string("mkdir -p ") + TestDir());
  }
  void TearDown() {
    System(string("rm -rf ") + TestDir());
  }
};

class FilesystemCopierActionTestDelegate : public ActionProcessorDelegate {
 public:
  FilesystemCopierActionTestDelegate() : ran_(false), success_(false) {}
  void ProcessingDone(const ActionProcessor* processor, bool success) {
    g_main_loop_quit(loop_);
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

gboolean StartProcessorInRunLoop(gpointer data) {
  ActionProcessor* processor = reinterpret_cast<ActionProcessor*>(data);
  processor->StartProcessing();
  return FALSE;
}

TEST_F(FilesystemCopierActionTest, RunAsRootSimpleTest) {
  ASSERT_EQ(0, getuid());
  DoTest(false, false);
}
void FilesystemCopierActionTest::DoTest(bool double_copy,
                                        bool run_out_of_space) {
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);

  // make two populated ext images, mount them both (one in the other),
  // and copy to a loop device setup to correspond to another file.

  const string a_image(TestDir() + "/a_image");
  const string b_image(TestDir() + "/b_image");
  const string out_image(TestDir() + "/out_image");

  vector<string> expected_paths_vector;
  CreateExtImageAtPath(a_image, &expected_paths_vector);
  CreateExtImageAtPath(b_image, NULL);

  // create 5 MiB file
  ASSERT_EQ(0, System(string("dd if=/dev/zero of=") + out_image
                      + " seek=5242879 bs=1 count=1"));

  // mount them both
  System(("mkdir -p " + TestDir() + "/mnt").c_str());
  ASSERT_EQ(0, System(string("mount -o loop ") + a_image + " " +
                      TestDir() + "/mnt"));
  ASSERT_EQ(0,
            System(string("mount -o loop ") + b_image + " " +
                   TestDir() + "/mnt/some_dir/mnt"));

  if (run_out_of_space)
    ASSERT_EQ(0, System(string("dd if=/dev/zero of=") +
                        TestDir() + "/mnt/big_zero bs=5M count=1"));

  string dev = GetUnusedLoopDevice();

  EXPECT_EQ(0, System(string("losetup ") + dev + " " + out_image));

  InstallPlan install_plan;
  install_plan.is_full_update = false;
  install_plan.install_path = dev;

  ActionProcessor processor;
  FilesystemCopierActionTestDelegate delegate;
  delegate.set_loop(loop);
  processor.set_delegate(&delegate);

  ObjectFeederAction<InstallPlan> feeder_action;
  FilesystemCopierAction copier_action;
  FilesystemCopierAction copier_action2;
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&feeder_action, &copier_action);
  if (double_copy) {
    BondActions(&copier_action, &copier_action2);
    BondActions(&copier_action2, &collector_action);
  } else {
    BondActions(&copier_action, &collector_action);
  }

  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&copier_action);
  if (double_copy)
    processor.EnqueueAction(&copier_action2);
  processor.EnqueueAction(&collector_action);

  copier_action.set_copy_source(TestDir() + "/mnt");
  feeder_action.set_obj(install_plan);

  g_timeout_add(0, &StartProcessorInRunLoop, &processor);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);

  EXPECT_EQ(0, System(string("losetup -d ") + dev));
  EXPECT_EQ(0, System(string("umount ") + TestDir() + "/mnt/some_dir/mnt"));
  EXPECT_EQ(0, System(string("umount ") + TestDir() + "/mnt"));
  EXPECT_EQ(0, unlink(a_image.c_str()));
  EXPECT_EQ(0, unlink(b_image.c_str()));

  EXPECT_TRUE(delegate.ran());
  if (run_out_of_space) {
    EXPECT_FALSE(delegate.success());
    EXPECT_EQ(0, unlink(out_image.c_str()));
    EXPECT_EQ(0, rmdir((TestDir() + "/mnt").c_str()));
    return;
  }
  EXPECT_TRUE(delegate.success());

  EXPECT_EQ(0, System(string("mount -o loop ") + out_image + " " +
                      TestDir() + "/mnt"));
  // Make sure everything in the out_image is there
  expected_paths_vector.push_back("/update_engine_copy_success");
  for (vector<string>::iterator it = expected_paths_vector.begin();
       it != expected_paths_vector.end(); ++it) {
    *it = TestDir() + "/mnt" + *it;
  }
  set<string> expected_paths(expected_paths_vector.begin(),
                             expected_paths_vector.end());
  VerifyAllPaths(TestDir() + "/mnt", expected_paths);
  string file_data;
  EXPECT_TRUE(utils::ReadFileToString(TestDir() + "/mnt/hi", &file_data));
  EXPECT_EQ("hi\n", file_data);
  EXPECT_TRUE(utils::ReadFileToString(TestDir() + "/mnt/hello", &file_data));
  EXPECT_EQ("hello\n", file_data);
  EXPECT_EQ("/some/target", Readlink(TestDir() + "/mnt/sym"));
  EXPECT_EQ(0, System(string("umount ") + TestDir() + "/mnt"));

  EXPECT_EQ(0, unlink(out_image.c_str()));
  EXPECT_EQ(0, rmdir((TestDir() + "/mnt").c_str()));

  EXPECT_FALSE(copier_action.skipped_copy());
  LOG(INFO) << "collected plan:";
  collector_action.object().Dump();
  LOG(INFO) << "expected plan:";
  install_plan.Dump();
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

  FilesystemCopierAction copier_action;
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
  InstallPlan install_plan(true, "", "", "");
  feeder_action.set_obj(install_plan);
  FilesystemCopierAction copier_action;
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
}

TEST_F(FilesystemCopierActionTest, NonExistentDriveTest) {
  ActionProcessor processor;
  FilesystemCopierActionTest2Delegate delegate;

  processor.set_delegate(&delegate);

  ObjectFeederAction<InstallPlan> feeder_action;
  InstallPlan install_plan(false, "", "", "/some/missing/file/path");
  feeder_action.set_obj(install_plan);
  FilesystemCopierAction copier_action;
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

TEST_F(FilesystemCopierActionTest, RunAsRootSkipUpdateTest) {
  ASSERT_EQ(0, getuid());
  DoTest(true, false);
}

TEST_F(FilesystemCopierActionTest, RunAsRootNoSpaceTest) {
  ASSERT_EQ(0, getuid());
  DoTest(false, true);
}

}  // namespace chromeos_update_engine
