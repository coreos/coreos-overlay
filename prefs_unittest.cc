// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>

#include <string>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "gtest/gtest.h"
#include "update_engine/prefs.h"

using std::string;

namespace chromeos_update_engine {

class PrefsTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(file_util::CreateNewTempDirectory("auprefs", &prefs_dir_));
    ASSERT_TRUE(prefs_.Init(prefs_dir_));
  }

  virtual void TearDown() {
    file_util::Delete(prefs_dir_, true);  // recursive
  }

  bool SetValue(const string& key, const string& value) {
    return file_util::WriteFile(prefs_dir_.Append(key),
                                value.data(), value.length()) ==
        static_cast<int>(value.length());
  }

  FilePath prefs_dir_;
  Prefs prefs_;
};

TEST_F(PrefsTest, GetFileNameForKey) {
  const char kKey[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_-";
  FilePath path;
  EXPECT_TRUE(prefs_.GetFileNameForKey(kKey, &path));
  EXPECT_EQ(prefs_dir_.Append(kKey).value(), path.value());
}

TEST_F(PrefsTest, GetFileNameForKeyBadCharacter) {
  FilePath path;
  EXPECT_FALSE(prefs_.GetFileNameForKey("ABC abc", &path));
}

TEST_F(PrefsTest, GetFileNameForKeyEmpty) {
  FilePath path;
  EXPECT_FALSE(prefs_.GetFileNameForKey("", &path));
}

TEST_F(PrefsTest, GetString) {
  const char kKey[] = "test-key";
  const string test_data = "test data";
  ASSERT_TRUE(SetValue(kKey, test_data));
  string value;
  EXPECT_TRUE(prefs_.GetString(kKey, &value));
  EXPECT_EQ(test_data, value);
}

TEST_F(PrefsTest, GetStringBadKey) {
  string value;
  EXPECT_FALSE(prefs_.GetString(",bad", &value));
}

TEST_F(PrefsTest, GetStringNonExistentKey) {
  string value;
  EXPECT_FALSE(prefs_.GetString("non-existent-key", &value));
}

TEST_F(PrefsTest, SetString) {
  const char kKey[] = "my_test_key";
  const char kValue[] = "some test value\non 2 lines";
  EXPECT_TRUE(prefs_.SetString(kKey, kValue));
  string value;
  EXPECT_TRUE(file_util::ReadFileToString(prefs_dir_.Append(kKey), &value));
  EXPECT_EQ(kValue, value);
}

TEST_F(PrefsTest, SetStringBadKey) {
  const char kKey[] = ".no-dots";
  EXPECT_FALSE(prefs_.SetString(kKey, "some value"));
  EXPECT_FALSE(file_util::PathExists(prefs_dir_.Append(kKey)));
}

TEST_F(PrefsTest, SetStringCreateDir) {
  const char kKey[] = "a-test-key";
  const char kValue[] = "test value";
  EXPECT_TRUE(prefs_.Init(FilePath(prefs_dir_.Append("subdir"))));
  EXPECT_TRUE(prefs_.SetString(kKey, kValue));
  string value;
  EXPECT_TRUE(
      file_util::ReadFileToString(prefs_dir_.Append("subdir").Append(kKey),
                                  &value));
  EXPECT_EQ(kValue, value);
}

TEST_F(PrefsTest, SetStringDirCreationFailure) {
  EXPECT_TRUE(prefs_.Init(FilePath("/dev/null")));
  const char kKey[] = "test-key";
  EXPECT_FALSE(prefs_.SetString(kKey, "test value"));
}

TEST_F(PrefsTest, SetStringFileCreationFailure) {
  const char kKey[] = "a-test-key";
  file_util::CreateDirectory(prefs_dir_.Append(kKey));
  EXPECT_FALSE(prefs_.SetString(kKey, "test value"));
  EXPECT_TRUE(file_util::DirectoryExists(prefs_dir_.Append(kKey)));
}

TEST_F(PrefsTest, GetInt64) {
  const char kKey[] = "test-key";
  ASSERT_TRUE(SetValue(kKey, " \n 25 \t "));
  int64_t value;
  EXPECT_TRUE(prefs_.GetInt64(kKey, &value));
  EXPECT_EQ(25, value);
}

TEST_F(PrefsTest, GetInt64BadValue) {
  const char kKey[] = "test-key";
  ASSERT_TRUE(SetValue(kKey, "30a"));
  int64_t value;
  EXPECT_FALSE(prefs_.GetInt64(kKey, &value));
}

TEST_F(PrefsTest, GetInt64Max) {
  const char kKey[] = "test-key";
  ASSERT_TRUE(SetValue(kKey, StringPrintf("%" PRIi64, kint64max)));
  int64_t value;
  EXPECT_TRUE(prefs_.GetInt64(kKey, &value));
  EXPECT_EQ(kint64max, value);
}

TEST_F(PrefsTest, GetInt64Min) {
  const char kKey[] = "test-key";
  ASSERT_TRUE(SetValue(kKey, StringPrintf("%" PRIi64, kint64min)));
  int64_t value;
  EXPECT_TRUE(prefs_.GetInt64(kKey, &value));
  EXPECT_EQ(kint64min, value);
}

TEST_F(PrefsTest, GetInt64Negative) {
  const char kKey[] = "test-key";
  ASSERT_TRUE(SetValue(kKey, " \t -100 \n "));
  int64_t value;
  EXPECT_TRUE(prefs_.GetInt64(kKey, &value));
  EXPECT_EQ(-100, value);
}

TEST_F(PrefsTest, GetInt64NonExistentKey) {
  int64_t value;
  EXPECT_FALSE(prefs_.GetInt64("random-key", &value));
}

TEST_F(PrefsTest, SetInt64) {
  const char kKey[] = "test_int";
  EXPECT_TRUE(prefs_.SetInt64(kKey, -123));
  string value;
  EXPECT_TRUE(file_util::ReadFileToString(prefs_dir_.Append(kKey), &value));
  EXPECT_EQ("-123", value);
}

TEST_F(PrefsTest, SetInt64BadKey) {
  const char kKey[] = "s p a c e s";
  EXPECT_FALSE(prefs_.SetInt64(kKey, 20));
  EXPECT_FALSE(file_util::PathExists(prefs_dir_.Append(kKey)));
}

TEST_F(PrefsTest, SetInt64Max) {
  const char kKey[] = "test-max-int";
  EXPECT_TRUE(prefs_.SetInt64(kKey, kint64max));
  string value;
  EXPECT_TRUE(file_util::ReadFileToString(prefs_dir_.Append(kKey), &value));
  EXPECT_EQ(StringPrintf("%" PRIi64, kint64max), value);
}

TEST_F(PrefsTest, SetInt64Min) {
  const char kKey[] = "test-min-int";
  EXPECT_TRUE(prefs_.SetInt64(kKey, kint64min));
  string value;
  EXPECT_TRUE(file_util::ReadFileToString(prefs_dir_.Append(kKey), &value));
  EXPECT_EQ(StringPrintf("%" PRIi64, kint64min), value);
}

}  // namespace chromeos_update_engine
