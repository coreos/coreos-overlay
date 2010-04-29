// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "update_engine/split_file_writer.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::min;
using std::string;
using std::vector;

namespace chromeos_update_engine {

class SplitFileWriterTest : public ::testing::Test {
 protected:
  void DoTest(size_t bytes_per_call);
};

void SplitFileWriterTest::DoTest(size_t bytes_per_call) {
  string first_file;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/SplitFileWriterTestFirst.XXXXXX",
                                  &first_file,
                                  NULL));
  ScopedPathUnlinker first_file_unlinker(first_file);
  string second_file;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/SplitFileWriterTestSecond.XXXXXX",
                                  &second_file,
                                  NULL));
  ScopedPathUnlinker second_file_unlinker(second_file);

  // Create joined data
  uint64_t first_bytes = 789;
  uint64_t second_bytes = 1000;

  vector<char> buf(sizeof(uint64_t) + first_bytes + second_bytes);
  uint64_t big_endian_first_bytes = htobe64(first_bytes);

  FillWithData(&buf);
  memcpy(&buf[0], &big_endian_first_bytes, sizeof(big_endian_first_bytes));
  
  // Create FileWriters
  DirectFileWriter first_file_writer;
  DirectFileWriter second_file_writer;
  SplitFileWriter split_file_writer(&first_file_writer, &second_file_writer);

  split_file_writer.SetFirstOpenArgs(
      first_file.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
  EXPECT_GE(split_file_writer.Open(
      second_file.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644), 0);
  ScopedFileWriterCloser closer(&split_file_writer);

  size_t bytes_written_total = 0;
  for (size_t i = 0; i < buf.size(); i += bytes_per_call) {
    size_t bytes_this_iteration = min(bytes_per_call,
                                      buf.size() - bytes_written_total);
    EXPECT_EQ(bytes_this_iteration,
              split_file_writer.Write(&buf[bytes_written_total],
                                      bytes_this_iteration));
    bytes_written_total += bytes_this_iteration;
  }
  EXPECT_EQ(buf.size(), bytes_written_total);

  vector<char> first_actual_data;
  vector<char> second_actual_data;
  EXPECT_TRUE(utils::ReadFile(first_file, &first_actual_data));
  EXPECT_TRUE(utils::ReadFile(second_file, &second_actual_data));
  
  EXPECT_EQ(first_bytes, first_actual_data.size());
  EXPECT_EQ(second_bytes, second_actual_data.size());

  vector<char> first_part_from_orig(&buf[sizeof(uint64_t)],
                                    &buf[sizeof(uint64_t) + first_bytes]);
  vector<char> second_part_from_orig(
      &buf[sizeof(uint64_t) + first_bytes],
      &buf[sizeof(uint64_t) + first_bytes + second_bytes]);

  EXPECT_TRUE(ExpectVectorsEq(first_part_from_orig, first_actual_data));
  EXPECT_TRUE(ExpectVectorsEq(second_part_from_orig, second_actual_data));
}

TEST_F(SplitFileWriterTest, SimpleTest) {
  DoTest(1024 * 1024 * 1024);  // Something very big
}

TEST_F(SplitFileWriterTest, OneByteAtATimeTest) {
  DoTest(1);
}

TEST_F(SplitFileWriterTest, ThreeBytesAtATimeTest) {
  DoTest(3);
}

}  // namespace chromeos_update_engine
