// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <utime.h>
#include <set>
#include <string>
#include <vector>
#include "base/string_util.h"
#include <gtest/gtest.h>
#include "update_engine/delta_diff_generator.h"
#include "update_engine/filesystem_iterator.h"
#include "update_engine/install_action.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using chromeos_update_engine::DeltaDiffGenerator;
using chromeos_update_engine::kRandomString;
using chromeos_update_engine::System;
using chromeos_update_engine::utils::WriteFile;
using std::set;
using std::string;
using std::vector;

namespace {
void GenerateFilesAtPath(const string& base) {
  EXPECT_EQ(0, System(StringPrintf("echo hi > %s/hi", base.c_str())));
  EXPECT_EQ(0, System(StringPrintf("ln %s/hi %s/hard_link", base.c_str(),
                                   base.c_str())));
  EXPECT_EQ(0, System(StringPrintf("mkdir -p %s/dir", base.c_str())));
  EXPECT_EQ(0, System(StringPrintf("echo hello > %s/dir/hello", base.c_str())));
  EXPECT_EQ(0, System(StringPrintf("echo -n > %s/dir/newempty", base.c_str())));
  EXPECT_EQ(0, System(StringPrintf("rm -f %s/dir/bdev", base.c_str())));
  EXPECT_EQ(0, System(StringPrintf("mknod %s/dir/bdev b 3 1", base.c_str())));
  EXPECT_EQ(0, System(StringPrintf("rm -f %s/cdev", base.c_str())));
  EXPECT_EQ(0, System(StringPrintf("mknod %s/cdev c 2 1", base.c_str())));
  EXPECT_EQ(0, System(StringPrintf("mkdir -p %s/dir/subdir", base.c_str())));
  EXPECT_EQ(0, System(StringPrintf("mkdir -p %s/dir/emptydir", base.c_str())));
  EXPECT_EQ(0, System(StringPrintf("echo -n foo > %s/dir/bigfile",
                                   base.c_str())));
  EXPECT_EQ(0, System(StringPrintf("chown 501:503 %s/dir/emptydir",
                                   base.c_str())));
  EXPECT_EQ(0, System(StringPrintf("rm -f %s/dir/subdir/fifo", base.c_str())));
  EXPECT_EQ(0, System(StringPrintf("mkfifo %s/dir/subdir/fifo", base.c_str())));
  EXPECT_EQ(0, System(StringPrintf("ln -f -s /target %s/dir/subdir/link",
                                   base.c_str())));
  EXPECT_TRUE(WriteFile((base + "/big_file").c_str(),
                        reinterpret_cast<const char*>(kRandomString),
                        sizeof(kRandomString)));
}

// Returns true if files at paths a, b are equal and there are no errors.
bool FilesEqual(const string& a, const string& b) {
  struct stat a_stbuf;
  struct stat b_stbuf;

  int r = lstat(a.c_str(), &a_stbuf);
  TEST_AND_RETURN_FALSE_ERRNO(r == 0);
  r = lstat(b.c_str(), &b_stbuf);
  TEST_AND_RETURN_FALSE_ERRNO(r == 0);

  TEST_AND_RETURN_FALSE(a_stbuf.st_mode == b_stbuf.st_mode);
  if (S_ISBLK(a_stbuf.st_mode) || S_ISCHR(a_stbuf.st_mode))
    TEST_AND_RETURN_FALSE(a_stbuf.st_rdev == b_stbuf.st_rdev);
  if (!S_ISDIR(a_stbuf.st_mode))
    TEST_AND_RETURN_FALSE(a_stbuf.st_nlink == b_stbuf.st_nlink);
  if (!S_ISREG(a_stbuf.st_mode)) {
    return true;
  }
  // Compare files
  TEST_AND_RETURN_FALSE(a_stbuf.st_size == b_stbuf.st_size);
  vector<char> a_data;
  TEST_AND_RETURN_FALSE(chromeos_update_engine::utils::ReadFile(a, &a_data));
  vector<char> b_data;
  TEST_AND_RETURN_FALSE(chromeos_update_engine::utils::ReadFile(b, &b_data));
  TEST_AND_RETURN_FALSE(a_data == b_data);
  return true;
}

class ScopedLoopDevUnmapper {
 public:
  explicit ScopedLoopDevUnmapper(const string& dev) : dev_(dev) {}
  ~ScopedLoopDevUnmapper() {
    EXPECT_EQ(0, System(string("losetup -d ") + dev_));
  }
 private:
  string dev_;
};

}

namespace chromeos_update_engine {

class InstallActionTest : public ::testing::Test { };

TEST(InstallActionTest, RunAsRootDiffTest) {
  ASSERT_EQ(0, getuid());
  string loop_dev = GetUnusedLoopDevice();
  ScopedLoopDevUnmapper loop_dev_unmapper(loop_dev);
  LOG(INFO) << "Using loop device: " << loop_dev;
  const string original_image("orig.image");
  const string original_dir("orig");
  const string new_dir("new");

  ASSERT_EQ(0, System(string("dd if=/dev/zero of=") + original_image +
                      " bs=5M count=1"));
  ASSERT_EQ(0, System(string("mkfs.ext3 -F ") + original_image));
  ASSERT_EQ(0, System(string("losetup ") + loop_dev + " " + original_image));
  ASSERT_EQ(0, System(string("mkdir ") + original_dir));
  ASSERT_EQ(0, System(string("mount ") + loop_dev + " " + original_dir));
  ASSERT_EQ(0, System(string("mkdir ") + new_dir));

  GenerateFilesAtPath(original_dir);
  GenerateFilesAtPath(new_dir);

  {
    // Fill bigfile w/ some data in the new folder
    vector<char> buf(100 * 1024);
    for (unsigned int i = 0; i < buf.size(); i++) {
      buf[i] = static_cast<char>(i);
    }
    EXPECT_TRUE(WriteFileVector(new_dir + "/dir/bigfile", buf));
  }
  const char* const new_dir_cstr = new_dir.c_str();
  EXPECT_EQ(0, System(StringPrintf("mkdir -p '%s/newdir'", new_dir_cstr)));
  EXPECT_EQ(0, System(StringPrintf("mkdir -p '%s/newdir/x'", new_dir_cstr)));
  EXPECT_EQ(0, System(StringPrintf("echo -n foo > '%s/newdir/x/file'",
                                   new_dir_cstr)));
  EXPECT_EQ(0, System(StringPrintf("echo -n x >> '%s/big_file'",
                                   new_dir_cstr)));
  // Make a symlink that compresses well:
  EXPECT_EQ(0, System(StringPrintf(
      "ln -s "
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx "
      "'%s/compress_sym'", new_dir_cstr)));

  // Make a device that will compress
  EXPECT_EQ(0, mknod((new_dir + "/bdev_gz").c_str(), S_IFCHR | 0644, 0));

  // Make a diff
  DeltaArchiveManifest* delta =
      DeltaDiffGenerator::EncodeMetadataToProtoBuffer(new_dir.c_str());
  EXPECT_TRUE(NULL != delta);
  EXPECT_TRUE(DeltaDiffGenerator::EncodeDataToDeltaFile(delta,
                                                        original_dir,
                                                        new_dir,
                                                        "delta",
                                                        new_dir + "/bdev_gz"));

  ASSERT_EQ(0, System(string("umount ") + original_dir));

  ObjectFeederAction<InstallPlan> feeder_action;
  InstallAction install_action;
  ObjectCollectorAction<string> collector_action;

  BondActions(&feeder_action, &install_action);
  BondActions(&install_action, &collector_action);

  ActionProcessor processor;
  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&install_action);
  processor.EnqueueAction(&collector_action);

  InstallPlan install_plan(false, "", "", "delta", loop_dev);
  feeder_action.set_obj(install_plan);

  processor.StartProcessing();
  EXPECT_FALSE(processor.IsRunning()) << "Update to handle async actions";

  EXPECT_EQ(loop_dev, collector_action.object());

  ASSERT_EQ(0, System(string("mount ") + loop_dev + " " + original_dir));

  // Check that original_dir and new_dir are equal
  int original_count = 0;
  LOG(INFO) << "checking old";
  {
    FilesystemIterator iter(original_dir,
                            utils::SetWithValue<string>("/lost+found"));
    for (; !iter.IsEnd(); iter.Increment()) {
      original_count++;
      LOG(INFO) << "checking path: " << iter.GetPartialPath();
      EXPECT_TRUE(FilesEqual(original_dir + iter.GetPartialPath(),
                             new_dir + iter.GetPartialPath()));
    }
    EXPECT_FALSE(iter.IsErr());
  }
  LOG(INFO) << "checking new";
  int new_count = 0;
  {
    FilesystemIterator iter(new_dir, set<string>());
    for (; !iter.IsEnd(); iter.Increment()) {
      new_count++;
      LOG(INFO) << "checking path: " << iter.GetPartialPath();
      EXPECT_TRUE(FilesEqual(original_dir + iter.GetPartialPath(),
                             new_dir + iter.GetPartialPath()));
    }
    EXPECT_FALSE(iter.IsErr());
  }
  LOG(INFO) << "new_count = " << new_count;
  EXPECT_EQ(new_count, original_count);
  EXPECT_EQ(19, original_count);

  // Make sure hard-link installed properly
  {
    struct stat hard_link_stbuf;
    struct stat hi_stbuf;
    EXPECT_EQ(0, lstat((string(new_dir) + "/hard_link").c_str(),
                       &hard_link_stbuf));
    EXPECT_EQ(0, lstat((string(new_dir) + "/hi").c_str(), &hi_stbuf));
    EXPECT_EQ(hard_link_stbuf.st_mode, hi_stbuf.st_mode);
    EXPECT_EQ(2, hard_link_stbuf.st_nlink);
    EXPECT_EQ(2, hi_stbuf.st_nlink);
    EXPECT_EQ(hi_stbuf.st_ino, hard_link_stbuf.st_ino);
  }

  EXPECT_EQ(0, System(string("umount ") + original_dir));

  // Cleanup generated files
  EXPECT_EQ(0, System(string("rm -rf ") + original_dir));
  EXPECT_EQ(0, System(string("rm -rf ") + new_dir));
  EXPECT_EQ(0, System(string("rm -f ") + original_image));
  EXPECT_EQ(0, system("rm -f delta"));
}

TEST(InstallActionTest, FullUpdateTest) {
  ObjectFeederAction<InstallPlan> feeder_action;
  InstallAction install_action;
  ObjectCollectorAction<string> collector_action;

  BondActions(&feeder_action, &install_action);
  BondActions(&install_action, &collector_action);

  ActionProcessor processor;
  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&install_action);
  processor.EnqueueAction(&collector_action);

  InstallPlan install_plan(true, "", "", "delta", "install_path");
  feeder_action.set_obj(install_plan);

  processor.StartProcessing();
  EXPECT_FALSE(processor.IsRunning()) << "Update to handle async actions";
  EXPECT_EQ("install_path", collector_action.object());
}

TEST(InstallActionTest, InvalidDeltaFileTest) {
  ObjectFeederAction<InstallPlan> feeder_action;
  InstallAction install_action;
  ObjectCollectorAction<string> collector_action;

  BondActions(&feeder_action, &install_action);
  BondActions(&install_action, &collector_action);

  ActionProcessor processor;
  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&install_action);
  processor.EnqueueAction(&collector_action);

  InstallPlan install_plan(false, "", "", "no_such_file", "install_path");
  feeder_action.set_obj(install_plan);

  processor.StartProcessing();
  EXPECT_FALSE(processor.IsRunning()) << "Update to handle async actions";
  EXPECT_TRUE(collector_action.object().empty());
}

}  // namespace chromeos_update_engine
