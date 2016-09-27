// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

#include <chrono>
#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "strings/string_printf.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::chrono::hours;
using std::chrono::minutes;
using std::chrono::seconds;
using std::chrono::microseconds;
using std::map;
using std::string;
using std::vector;
using strings::StringPrintf;

namespace chromeos_update_engine {

class UtilsTest : public ::testing::Test { };

TEST(UtilsTest, IsOfficialBuild) {
  // Pretty lame test...
  EXPECT_TRUE(utils::IsOfficialBuild());
}

TEST(UtilsTest, IsHTTPS) {
  EXPECT_TRUE(utils::IsHTTPS("https://"));
  EXPECT_TRUE(utils::IsHTTPS("https://host"));
  EXPECT_TRUE(utils::IsHTTPS("HTTPS://host"));
  EXPECT_FALSE(utils::IsHTTPS("http://host"));
  EXPECT_FALSE(utils::IsHTTPS("bogus"));
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
  for (const int value : collection) {
    EXPECT_EQ(expected_values[index++], value);
  }
}

TEST(UtilsTest, GetDeviceSizeTest) {
  string img;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/img.XXXXXX", &img, NULL));
  ScopedPathUnlinker img_unlinker(img);
  EXPECT_EQ(0, truncate(img.c_str(), 4096));

  off_t size = 0;
  EXPECT_TRUE(utils::GetDeviceSize(img, &size));
  EXPECT_EQ(4096, size);
}

TEST(UtilsTest, RunAsRootGetDeviceSizeTest) {
  string img, dev;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/img.XXXXXX", &img, NULL));
  ScopedPathUnlinker img_unlinker(img);
  EXPECT_EQ(0, truncate(img.c_str(), 4096));

  off_t size = 0;
  EXPECT_TRUE(utils::GetDeviceSize(img, &size));
  EXPECT_EQ(4096, size);

  size = 0;
  ScopedLoopbackDeviceBinder loop(img, &dev);
  EXPECT_TRUE(utils::GetDeviceSize(dev, &size));
  EXPECT_EQ(4096, size);
}

TEST(UtilsTest, DurationToStringTest) {
  EXPECT_EQ("1d0h0m0s", utils::ToString(utils::days_t(1)));
  EXPECT_EQ("1h0m0s", utils::ToString(hours(1)));
  EXPECT_EQ("1m0s", utils::ToString(minutes(1)));
  EXPECT_EQ("1s", utils::ToString(seconds(1)));
  EXPECT_EQ("0.000001s", utils::ToString(microseconds(1)));
  EXPECT_EQ("23h59m59s", utils::ToString(seconds(86399)));
}

}  // namespace chromeos_update_engine
