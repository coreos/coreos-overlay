// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <set>
#include <string>
#include <vector>
#include "base/string_util.h"
#include <gtest/gtest.h>
#include "update_engine/filesystem_iterator.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::set;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
const char* TestDir() { return "./FilesystemIteratorTest-dir"; }
}  // namespace {}

class FilesystemIteratorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    LOG(INFO) << "SetUp() mkdir";
    EXPECT_EQ(0, System(StringPrintf("rm -rf %s", TestDir())));
    EXPECT_EQ(0, System(StringPrintf("mkdir -p %s", TestDir())));
  }
  virtual void TearDown() {
    LOG(INFO) << "TearDown() rmdir";
    EXPECT_EQ(0, System(StringPrintf("rm -rf %s", TestDir())));
  }
};

TEST_F(FilesystemIteratorTest, RunAsRootSuccessTest) {
  ASSERT_EQ(0, getuid());
  string first_image("FilesystemIteratorTest.image1");
  string sub_image("FilesystemIteratorTest.image2");

  ASSERT_EQ(0, System(string("rm -f ") + first_image + " " + sub_image));
  vector<string> expected_paths_vector;
  CreateExtImageAtPath(first_image, &expected_paths_vector);
  CreateExtImageAtPath(sub_image, NULL);
  ASSERT_EQ(0, System(string("mount -o loop ") + first_image + " " +
                      kMountPath));
  ASSERT_EQ(0, System(string("mount -o loop ") + sub_image + " " +
                      kMountPath + "/some_dir/mnt"));
  for (vector<string>::iterator it = expected_paths_vector.begin();
       it != expected_paths_vector.end(); ++it)
    *it = kMountPath + *it;
  set<string> expected_paths(expected_paths_vector.begin(),
                             expected_paths_vector.end());
  VerifyAllPaths(kMountPath, expected_paths);
  
  EXPECT_EQ(0, System(string("umount ") + kMountPath + "/some_dir/mnt"));
  EXPECT_EQ(0, System(string("umount ") + kMountPath));
  EXPECT_EQ(0, System(string("rm -f ") + first_image + " " + sub_image));
}

TEST_F(FilesystemIteratorTest, NegativeTest) {
  {
    FilesystemIterator iter("/non/existent/path", set<string>());
    EXPECT_TRUE(iter.IsEnd());
    EXPECT_TRUE(iter.IsErr());
  }

  {
    FilesystemIterator iter(TestDir(), set<string>());
    EXPECT_FALSE(iter.IsEnd());
    EXPECT_FALSE(iter.IsErr());
    // Here I'm deleting the exact directory that iterator is point at,
    // then incrementing (which normally would descend into that directory).
    EXPECT_EQ(0, rmdir(TestDir()));
    iter.Increment();
    EXPECT_TRUE(iter.IsEnd());
    EXPECT_FALSE(iter.IsErr());
  }
}

TEST_F(FilesystemIteratorTest, DeleteWhileTraverseTest) {
  ASSERT_EQ(0, mkdir("DeleteWhileTraverseTest", 0755));
  ASSERT_EQ(0, mkdir("DeleteWhileTraverseTest/a", 0755));
  ASSERT_EQ(0, mkdir("DeleteWhileTraverseTest/a/b", 0755));
  ASSERT_EQ(0, mkdir("DeleteWhileTraverseTest/b", 0755));
  ASSERT_EQ(0, mkdir("DeleteWhileTraverseTest/c", 0755));

  string expected_paths_arr[] = {
    "",
    "/a",
    "/b",
    "/c"
  };
  set<string> expected_paths(expected_paths_arr,
                             expected_paths_arr +
                             arraysize(expected_paths_arr));

  FilesystemIterator iter("DeleteWhileTraverseTest", set<string>());
  while (!iter.IsEnd()) {
    string path = iter.GetPartialPath();
    EXPECT_TRUE(expected_paths.find(path) != expected_paths.end());
    if (expected_paths.find(path) != expected_paths.end()) {
      expected_paths.erase(path);
    }
    if (path == "/a") {
      EXPECT_EQ(0, rmdir("DeleteWhileTraverseTest/a/b"));
      EXPECT_EQ(0, rmdir("DeleteWhileTraverseTest/a"));
    }
    iter.Increment();
  }
  EXPECT_FALSE(iter.IsErr());
  EXPECT_TRUE(expected_paths.empty());
  EXPECT_EQ(0, system("rm -rf DeleteWhileTraverseTest"));
}

}  // namespace chromeos_update_engine
