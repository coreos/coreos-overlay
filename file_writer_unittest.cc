// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "update_engine/file_writer.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

class FileWriterTest : public ::testing::Test { };

TEST(FileWriterTest, SimpleTest) {
  DirectFileWriter file_writer;
  const string path("/tmp/FileWriterTest");
  ASSERT_EQ(0, file_writer.Open(path.c_str(),
                                O_CREAT | O_LARGEFILE | O_TRUNC | O_WRONLY,
                                0644));
  ASSERT_EQ(4, file_writer.Write("test", 4));
  vector<char> actual_data;
  EXPECT_TRUE(utils::ReadFile(path, &actual_data));

  EXPECT_FALSE(memcmp("test", &actual_data[0], actual_data.size()));
  EXPECT_EQ(0, file_writer.Close());
  unlink(path.c_str());
}

TEST(FileWriterTest, ErrorTest) {
  DirectFileWriter file_writer;
  const string path("/tmp/ENOENT/FileWriterTest");
  EXPECT_EQ(-ENOENT, file_writer.Open(path.c_str(),
                                      O_CREAT | O_LARGEFILE | O_TRUNC, 0644));
}

TEST(FileWriterTest, WriteErrorTest) {
  DirectFileWriter file_writer;
  const string path("/tmp/FileWriterTest");
  EXPECT_EQ(0, file_writer.Open(path.c_str(),
                                O_CREAT | O_LARGEFILE | O_TRUNC | O_RDONLY,
                                0644));
  EXPECT_EQ(-EBADF, file_writer.Write("x", 1));
  EXPECT_EQ(0, file_writer.Close());
}


}  // namespace chromeos_update_engine
