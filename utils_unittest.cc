// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "update_engine/utils.h"

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
}

TEST(UtilsTest, PartitionNumberTest) {
  EXPECT_EQ("3", utils::PartitionNumber("/dev/sda3"));
  EXPECT_EQ("3", utils::PartitionNumber("/dev/mmc0p3"));
}

}  // namespace chromeos_update_engine
