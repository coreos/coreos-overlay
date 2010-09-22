// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "update_engine/buffered_file_writer.h"
#include "update_engine/file_writer_mock.h"

using std::string;
using testing::_;
using testing::InSequence;
using testing::Return;

namespace chromeos_update_engine {

static const int kTestFlags = O_CREAT | O_LARGEFILE | O_TRUNC | O_WRONLY;
static const mode_t kTestMode = 0644;

MATCHER_P2(MemCmp, expected, n, "") { return memcmp(arg, expected, n) == 0; }

class BufferedFileWriterTest : public ::testing::Test {
 protected:
  FileWriterMock file_writer_mock_;
};

TEST_F(BufferedFileWriterTest, CloseErrorTest) {
  BufferedFileWriter writer(&file_writer_mock_, 100);
  writer.buffered_bytes_ = 1;
  InSequence s;
  EXPECT_CALL(file_writer_mock_, Write(&writer.buffer_[0], 1))
      .Times(1)
      .WillOnce(Return(50));
  EXPECT_CALL(file_writer_mock_, Close()).Times(1).WillOnce(Return(-20));
  EXPECT_EQ(-20, writer.Close());
}

TEST_F(BufferedFileWriterTest, CloseNoDataTest) {
  BufferedFileWriter writer(&file_writer_mock_, 50);
  InSequence s;
  EXPECT_CALL(file_writer_mock_, Write(_, _)).Times(0);
  EXPECT_CALL(file_writer_mock_, Close()).Times(1).WillOnce(Return(-30));
  EXPECT_EQ(-30, writer.Close());
}

TEST_F(BufferedFileWriterTest, CloseWriteErrorTest) {
  BufferedFileWriter writer(&file_writer_mock_, 100);
  writer.buffered_bytes_ = 2;
  InSequence s;
  EXPECT_CALL(file_writer_mock_, Write(&writer.buffer_[0], 2))
      .Times(1)
      .WillOnce(Return(-10));
  EXPECT_CALL(file_writer_mock_, Close()).Times(1).WillOnce(Return(-20));
  EXPECT_EQ(-10, writer.Close());
}

TEST_F(BufferedFileWriterTest, ConstructorTest) {
  BufferedFileWriter writer(&file_writer_mock_, 123);
  EXPECT_EQ(&file_writer_mock_, writer.next_);
  EXPECT_EQ(123, writer.buffer_size_);
  EXPECT_TRUE(writer.buffer_ != NULL);
  EXPECT_EQ(0, writer.buffered_bytes_);
}

TEST_F(BufferedFileWriterTest, OpenTest) {
  BufferedFileWriter writer(&file_writer_mock_, 100);
  EXPECT_CALL(file_writer_mock_, Open("foo", kTestFlags, kTestMode))
      .Times(1)
      .WillOnce(Return(20));
  EXPECT_EQ(20, writer.Open("foo", kTestFlags, kTestMode));
}

TEST_F(BufferedFileWriterTest, WriteBufferNoDataTest) {
  BufferedFileWriter writer(&file_writer_mock_, 300);
  EXPECT_CALL(file_writer_mock_, Write(_, _)).Times(0);
  EXPECT_EQ(0, writer.WriteBuffer());
}

TEST_F(BufferedFileWriterTest, WriteBufferTest) {
  BufferedFileWriter writer(&file_writer_mock_, 200);
  writer.buffered_bytes_ = 100;
  EXPECT_CALL(file_writer_mock_, Write(&writer.buffer_[0], 100))
      .Times(1)
      .WillOnce(Return(-10));
  EXPECT_EQ(-10, writer.WriteBuffer());
}

TEST_F(BufferedFileWriterTest, WriteErrorTest) {
  string kData = "test_data";
  BufferedFileWriter writer(&file_writer_mock_, 4);
  InSequence s;
  EXPECT_CALL(file_writer_mock_, Write(MemCmp("test", 4), 4))
      .Times(1)
      .WillOnce(Return(3));
  EXPECT_CALL(file_writer_mock_, Write(MemCmp("_dat", 4), 4))
      .Times(1)
      .WillOnce(Return(-10));
  EXPECT_EQ(-10, writer.Write(kData.data(), kData.size()));
  EXPECT_EQ(0, writer.buffered_bytes_);
}

TEST_F(BufferedFileWriterTest, WriteTest) {
  string kData = "test_data";
  BufferedFileWriter writer(&file_writer_mock_, 100);
  writer.buffered_bytes_ = 10;
  EXPECT_CALL(file_writer_mock_, Write(_, _)).Times(0);
  EXPECT_EQ(kData.size(), writer.Write(kData.data(), kData.size()));
  EXPECT_EQ(0, memcmp(kData.data(), &writer.buffer_[10], kData.size()));
  EXPECT_EQ(kData.size() + 10, writer.buffered_bytes_);
}

TEST_F(BufferedFileWriterTest, WriteWrapBufferTest) {
  string kData = "test_data1";
  BufferedFileWriter writer(&file_writer_mock_, 3);
  writer.buffered_bytes_ = 1;
  writer.buffer_[0] = 'x';
  InSequence s;
  EXPECT_CALL(file_writer_mock_, Write(MemCmp("xte", 3), 3))
      .Times(1)
      .WillOnce(Return(3));
  EXPECT_CALL(file_writer_mock_, Write(MemCmp("st_", 3), 3))
      .Times(1)
      .WillOnce(Return(3));
  EXPECT_CALL(file_writer_mock_, Write(MemCmp("dat", 3), 3))
      .Times(1)
      .WillOnce(Return(3));
  EXPECT_EQ(kData.size(), writer.Write(kData.data(), kData.size()));
  EXPECT_EQ(0, memcmp("a1", &writer.buffer_[0], 2));
  EXPECT_EQ(2, writer.buffered_bytes_);
}

}  // namespace chromeos_update_engine
