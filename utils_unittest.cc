// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include <map>
#include <string>
#include <vector>

#include <base/string_util.h>
#include <gtest/gtest.h>

#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::map;
using std::string;
using std::vector;

namespace chromeos_update_engine {

class UtilsTest : public ::testing::Test { };

TEST(UtilsTest, IsOfficialBuild) {
  // Pretty lame test...
  EXPECT_TRUE(utils::IsOfficialBuild());
}

TEST(UtilsTest, NormalizePathTest) {
  EXPECT_EQ("", utils::NormalizePath("", false));
  EXPECT_EQ("", utils::NormalizePath("", true));
  EXPECT_EQ("/", utils::NormalizePath("/", false));
  EXPECT_EQ("", utils::NormalizePath("/", true));
  EXPECT_EQ("/", utils::NormalizePath("//", false));
  EXPECT_EQ("", utils::NormalizePath("//", true));
  EXPECT_EQ("foo", utils::NormalizePath("foo", false));
  EXPECT_EQ("foo", utils::NormalizePath("foo", true));
  EXPECT_EQ("/foo/", utils::NormalizePath("/foo//", false));
  EXPECT_EQ("/foo", utils::NormalizePath("/foo//", true));
  EXPECT_EQ("bar/baz/foo/adlr", utils::NormalizePath("bar/baz//foo/adlr",
                                                     false));
  EXPECT_EQ("bar/baz/foo/adlr", utils::NormalizePath("bar/baz//foo/adlr",
                                                     true));
  EXPECT_EQ("/bar/baz/foo/adlr/", utils::NormalizePath("/bar/baz//foo/adlr/",
                                                       false));
  EXPECT_EQ("/bar/baz/foo/adlr", utils::NormalizePath("/bar/baz//foo/adlr/",
                                                      true));
  EXPECT_EQ("\\\\", utils::NormalizePath("\\\\", false));
  EXPECT_EQ("\\\\", utils::NormalizePath("\\\\", true));
  EXPECT_EQ("\\:/;$PATH\n\\", utils::NormalizePath("\\://;$PATH\n\\", false));
  EXPECT_EQ("\\:/;$PATH\n\\", utils::NormalizePath("\\://;$PATH\n\\", true));
  EXPECT_EQ("/spaces s/ ok/s / / /",
            utils::NormalizePath("/spaces s/ ok/s / / /", false));
  EXPECT_EQ("/spaces s/ ok/s / / ",
            utils::NormalizePath("/spaces s/ ok/s / / /", true));
}

TEST(UtilsTest, ReadFileFailure) {
  vector<char> empty;
  EXPECT_FALSE(utils::ReadFile("/this/doesn't/exist", &empty));
}

TEST(UtilsTest, ErrnoNumberAsStringTest) {
  EXPECT_EQ("No such file or directory", utils::ErrnoNumberAsString(ENOENT));
}

TEST(UtilsTest, StringHasSuffixTest) {
  EXPECT_TRUE(utils::StringHasSuffix("foo", "foo"));
  EXPECT_TRUE(utils::StringHasSuffix("foo", "o"));
  EXPECT_TRUE(utils::StringHasSuffix("", ""));
  EXPECT_TRUE(utils::StringHasSuffix("abcabc", "abc"));
  EXPECT_TRUE(utils::StringHasSuffix("adlrwashere", "ere"));
  EXPECT_TRUE(utils::StringHasSuffix("abcdefgh", "gh"));
  EXPECT_TRUE(utils::StringHasSuffix("abcdefgh", ""));
  EXPECT_FALSE(utils::StringHasSuffix("foo", "afoo"));
  EXPECT_FALSE(utils::StringHasSuffix("", "x"));
  EXPECT_FALSE(utils::StringHasSuffix("abcdefgh", "fg"));
  EXPECT_FALSE(utils::StringHasSuffix("abcdefgh", "ab"));
}

TEST(UtilsTest, StringHasPrefixTest) {
  EXPECT_TRUE(utils::StringHasPrefix("foo", "foo"));
  EXPECT_TRUE(utils::StringHasPrefix("foo", "f"));
  EXPECT_TRUE(utils::StringHasPrefix("", ""));
  EXPECT_TRUE(utils::StringHasPrefix("abcabc", "abc"));
  EXPECT_TRUE(utils::StringHasPrefix("adlrwashere", "adl"));
  EXPECT_TRUE(utils::StringHasPrefix("abcdefgh", "ab"));
  EXPECT_TRUE(utils::StringHasPrefix("abcdefgh", ""));
  EXPECT_FALSE(utils::StringHasPrefix("foo", "fooa"));
  EXPECT_FALSE(utils::StringHasPrefix("", "x"));
  EXPECT_FALSE(utils::StringHasPrefix("abcdefgh", "bc"));
  EXPECT_FALSE(utils::StringHasPrefix("abcdefgh", "gh"));
}

TEST(UtilsTest, BootDeviceTest) {
  // Pretty lame test...
  EXPECT_FALSE(utils::BootDevice().empty());
}

TEST(UtilsTest, BootKernelDeviceTest) {
  EXPECT_EQ("", utils::BootKernelDevice("foo"));
  EXPECT_EQ("", utils::BootKernelDevice("/dev/sda0"));
  EXPECT_EQ("", utils::BootKernelDevice("/dev/sda1"));
  EXPECT_EQ("", utils::BootKernelDevice("/dev/sda2"));
  EXPECT_EQ("/dev/sda2", utils::BootKernelDevice("/dev/sda3"));
  EXPECT_EQ("", utils::BootKernelDevice("/dev/sda4"));
  EXPECT_EQ("/dev/sda4", utils::BootKernelDevice("/dev/sda5"));
  EXPECT_EQ("", utils::BootKernelDevice("/dev/sda6"));
  EXPECT_EQ("/dev/sda6", utils::BootKernelDevice("/dev/sda7"));
  EXPECT_EQ("", utils::BootKernelDevice("/dev/sda8"));
  EXPECT_EQ("", utils::BootKernelDevice("/dev/sda9"));
}

TEST(UtilsTest, RecursiveUnlinkDirTest) {
  EXPECT_EQ(0, mkdir("RecursiveUnlinkDirTest-a", 0755));
  EXPECT_EQ(0, mkdir("RecursiveUnlinkDirTest-b", 0755));
  EXPECT_EQ(0, symlink("../RecursiveUnlinkDirTest-a",
                       "RecursiveUnlinkDirTest-b/link"));
  EXPECT_EQ(0, system("echo hi > RecursiveUnlinkDirTest-b/file"));
  EXPECT_EQ(0, mkdir("RecursiveUnlinkDirTest-b/dir", 0755));
  EXPECT_EQ(0, system("echo ok > RecursiveUnlinkDirTest-b/dir/subfile"));
  EXPECT_TRUE(utils::RecursiveUnlinkDir("RecursiveUnlinkDirTest-b"));
  EXPECT_TRUE(utils::FileExists("RecursiveUnlinkDirTest-a"));
  EXPECT_EQ(0, system("rm -rf RecursiveUnlinkDirTest-a"));
  EXPECT_FALSE(utils::FileExists("RecursiveUnlinkDirTest-b"));
  EXPECT_TRUE(utils::RecursiveUnlinkDir("/something/that/doesnt/exist"));
}

TEST(UtilsTest, IsSymlinkTest) {
  string temp_dir;
  EXPECT_TRUE(utils::MakeTempDirectory("/tmp/symlink-test.XXXXXX", &temp_dir));
  string temp_file = temp_dir + "temp-file";
  EXPECT_TRUE(utils::WriteFile(temp_file.c_str(), "", 0));
  string temp_symlink = temp_dir + "temp-symlink";
  EXPECT_EQ(0, symlink(temp_file.c_str(), temp_symlink.c_str()));
  EXPECT_FALSE(utils::IsSymlink(temp_dir.c_str()));
  EXPECT_FALSE(utils::IsSymlink(temp_file.c_str()));
  EXPECT_TRUE(utils::IsSymlink(temp_symlink.c_str()));
  EXPECT_FALSE(utils::IsSymlink("/non/existent/path"));
  EXPECT_TRUE(utils::RecursiveUnlinkDir(temp_dir));
}

TEST(UtilsTest, TempFilenameTest) {
  const string original = "/foo.XXXXXX";
  const string result = utils::TempFilename(original);
  EXPECT_EQ(original.size(), result.size());
  EXPECT_TRUE(utils::StringHasPrefix(result, "/foo."));
  EXPECT_FALSE(utils::StringHasSuffix(result, "XXXXXX"));
}

TEST(UtilsTest, RootDeviceTest) {
  EXPECT_EQ("/dev/sda", utils::RootDevice("/dev/sda3"));
  EXPECT_EQ("/dev/mmc0", utils::RootDevice("/dev/mmc0p3"));
  EXPECT_EQ("", utils::RootDevice("/dev/foo/bar"));
  EXPECT_EQ("", utils::RootDevice("/"));
  EXPECT_EQ("", utils::RootDevice(""));
}

TEST(UtilsTest, SysfsBlockDeviceTest) {
  EXPECT_EQ("/sys/block/sda", utils::SysfsBlockDevice("/dev/sda"));
  EXPECT_EQ("", utils::SysfsBlockDevice("/foo/sda"));
  EXPECT_EQ("", utils::SysfsBlockDevice("/dev/foo/bar"));
  EXPECT_EQ("", utils::SysfsBlockDevice("/"));
  EXPECT_EQ("", utils::SysfsBlockDevice("./"));
  EXPECT_EQ("", utils::SysfsBlockDevice(""));
}

TEST(UtilsTest, IsRemovableDeviceTest) {
  EXPECT_FALSE(utils::IsRemovableDevice(""));
  EXPECT_FALSE(utils::IsRemovableDevice("/dev/non-existent-device"));
}

TEST(UtilsTest, PartitionNumberTest) {
  EXPECT_EQ("3", utils::PartitionNumber("/dev/sda3"));
  EXPECT_EQ("3", utils::PartitionNumber("/dev/mmc0p3"));
}


TEST(UtilsTest, RunAsRootSetProcessPriorityTest) {
  // getpriority may return -1 on error so the getpriority logic needs to be
  // enhanced if any of the pre-defined priority constants are changed to -1.
  ASSERT_NE(-1, utils::kProcessPriorityLow);
  ASSERT_NE(-1, utils::kProcessPriorityNormal);
  ASSERT_NE(-1, utils::kProcessPriorityHigh);
  EXPECT_EQ(utils::kProcessPriorityNormal, getpriority(PRIO_PROCESS, 0));
  EXPECT_TRUE(utils::SetProcessPriority(utils::kProcessPriorityHigh));
  EXPECT_EQ(utils::kProcessPriorityHigh, getpriority(PRIO_PROCESS, 0));
  EXPECT_TRUE(utils::SetProcessPriority(utils::kProcessPriorityLow));
  EXPECT_EQ(utils::kProcessPriorityLow, getpriority(PRIO_PROCESS, 0));
  EXPECT_TRUE(utils::SetProcessPriority(utils::kProcessPriorityNormal));
  EXPECT_EQ(utils::kProcessPriorityNormal, getpriority(PRIO_PROCESS, 0));
}

TEST(UtilsTest, ComparePrioritiesTest) {
  EXPECT_LT(utils::ComparePriorities(utils::kProcessPriorityLow,
                                     utils::kProcessPriorityNormal), 0);
  EXPECT_GT(utils::ComparePriorities(utils::kProcessPriorityNormal,
                                     utils::kProcessPriorityLow), 0);
  EXPECT_EQ(utils::ComparePriorities(utils::kProcessPriorityNormal,
                                     utils::kProcessPriorityNormal), 0);
  EXPECT_GT(utils::ComparePriorities(utils::kProcessPriorityHigh,
                                     utils::kProcessPriorityNormal), 0);
}

TEST(UtilsTest, FuzzIntTest) {
  static const unsigned int kRanges[] = { 0, 1, 2, 20 };
  for (size_t r = 0; r < arraysize(kRanges); ++r) {
    unsigned int range = kRanges[r];
    const int kValue = 50;
    for (int tries = 0; tries < 100; ++tries) {
      int value = utils::FuzzInt(kValue, range);
      EXPECT_GE(value, kValue - range / 2);
      EXPECT_LE(value, kValue + range - range / 2);
    }
  }
}

TEST(UtilsTest, ApplyMapTest) {
  int initial_values[] = {1, 2, 3, 4, 6};
  vector<int> collection(&initial_values[0],
                         initial_values + arraysize(initial_values));
  EXPECT_EQ(arraysize(initial_values), collection.size());
  int expected_values[] = {1, 2, 5, 4, 8};
  map<int, int> value_map;
  value_map[3] = 5;
  value_map[6] = 8;
  value_map[5] = 10;

  utils::ApplyMap(&collection, value_map);

  size_t index = 0;
  for (vector<int>::iterator it = collection.begin(), e = collection.end();
       it != e; ++it) {
    EXPECT_EQ(expected_values[index++], *it);
  }
}

TEST(UtilsTest, RunAsRootGetFilesystemSizeTest) {
  string img;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/img.XXXXXX", &img, NULL));
  ScopedPathUnlinker img_unlinker(img);
  CreateExtImageAtPath(img, NULL);
  // Extend the "partition" holding the file system from 10MiB to 20MiB.
  EXPECT_EQ(0, System(base::StringPrintf(
      "dd if=/dev/zero of=%s seek=20971519 bs=1 count=1",
      img.c_str())));
  EXPECT_EQ(20 * 1024 * 1024, utils::FileSize(img));
  int block_count = 0;
  int block_size = 0;
  EXPECT_TRUE(utils::GetFilesystemSize(img, &block_count, &block_size));
  EXPECT_EQ(4096, block_size);
  EXPECT_EQ(10 * 1024 * 1024 / 4096, block_count);
}

namespace {
gboolean  TerminateScheduleCrashReporterUploadTest(void* arg) {
  GMainLoop* loop = reinterpret_cast<GMainLoop*>(arg);
  g_main_loop_quit(loop);
  return FALSE;  // Don't call this callback again
}
}  // namespace {}

TEST(UtilsTest, ScheduleCrashReporterUploadTest) {
  // Not much to test. At least this tests for memory leaks, crashes,
  // log errors.
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  utils::ScheduleCrashReporterUpload();
  g_timeout_add_seconds(1, &TerminateScheduleCrashReporterUploadTest, loop);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
}

}  // namespace chromeos_update_engine
