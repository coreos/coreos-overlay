// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "update_engine/extent_writer.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::min;
using std::string;
using std::vector;

namespace chromeos_update_engine {

COMPILE_ASSERT(sizeof(off_t) == 8, off_t_not_64_bit);

namespace {
const char kPathTemplate[] = "./ExtentWriterTest-file.XXXXXX";
const size_t kBlockSize = 4096;
}

class ExtentWriterTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    memcpy(path_, kPathTemplate, sizeof(kPathTemplate));
    fd_ = mkstemp(path_);
    ASSERT_GE(fd_, 0);
  }
  virtual void TearDown() {
    close(fd_);
    unlink(path_);
  }
  int fd() { return fd_; }
  const char* path() { return path_; }
  
  // Writes data to an extent writer in 'chunk_size' chunks with
  // the first chunk of size first_chunk_size. It calculates what the
  // resultant file should look like and ensure that the extent writer
  // wrote the file correctly.
  void WriteAlignedExtents(size_t chunk_size, size_t first_chunk_size);
  void TestZeroPad(bool aligned_size);
 private:
  int fd_;
  char path_[sizeof(kPathTemplate)];
};

TEST_F(ExtentWriterTest, SimpleTest) {
  vector<Extent> extents;
  Extent extent;
  extent.set_start_block(1);
  extent.set_num_blocks(1);
  extents.push_back(extent);
  
  const string bytes = "1234";

  DirectExtentWriter direct_writer;
  EXPECT_TRUE(direct_writer.Init(fd(), extents, kBlockSize));
  EXPECT_TRUE(direct_writer.Write(bytes.data(), bytes.size()));
  EXPECT_TRUE(direct_writer.End());
  
  struct stat stbuf;
  EXPECT_EQ(0, fstat(fd(), &stbuf));
  EXPECT_EQ(kBlockSize + bytes.size(), stbuf.st_size);
  
  vector<char> result_file;
  EXPECT_TRUE(utils::ReadFile(path(), &result_file));
  
  vector<char> expected_file(kBlockSize);
  expected_file.insert(expected_file.end(),
                       bytes.data(), bytes.data() + bytes.size());
  ExpectVectorsEq(expected_file, result_file);
}

TEST_F(ExtentWriterTest, ZeroLengthTest) {
  vector<Extent> extents;
  Extent extent;
  extent.set_start_block(1);
  extent.set_num_blocks(1);
  extents.push_back(extent);

  DirectExtentWriter direct_writer;
  EXPECT_TRUE(direct_writer.Init(fd(), extents, kBlockSize));
  EXPECT_TRUE(direct_writer.Write(NULL, 0));
  EXPECT_TRUE(direct_writer.End());
}

TEST_F(ExtentWriterTest, OverflowExtentTest) {
  WriteAlignedExtents(kBlockSize * 3, kBlockSize * 3);
}

TEST_F(ExtentWriterTest, UnalignedWriteTest) {
  WriteAlignedExtents(7, 7);
}

TEST_F(ExtentWriterTest, LargeUnalignedWriteTest) {
  WriteAlignedExtents(kBlockSize * 2, kBlockSize / 2);
}

void ExtentWriterTest::WriteAlignedExtents(size_t chunk_size,
                                           size_t first_chunk_size) {
  vector<Extent> extents;
  Extent extent;
  extent.set_start_block(1);
  extent.set_num_blocks(1);
  extents.push_back(extent);
  extent.set_start_block(0);
  extent.set_num_blocks(1);
  extents.push_back(extent);
  extent.set_start_block(2);
  extent.set_num_blocks(1);
  extents.push_back(extent);
  
  vector<char> data(kBlockSize * 3);
  FillWithData(&data);
  
  DirectExtentWriter direct_writer;
  EXPECT_TRUE(direct_writer.Init(fd(), extents, kBlockSize));
  
  size_t bytes_written = 0;
  while (bytes_written < data.size()) {
    size_t bytes_to_write = min(data.size() - bytes_written, chunk_size);
    if (bytes_written == 0) {
      bytes_to_write = min(data.size() - bytes_written, first_chunk_size);
    }
    EXPECT_TRUE(direct_writer.Write(&data[bytes_written], bytes_to_write));
    bytes_written += bytes_to_write;
  }
  EXPECT_TRUE(direct_writer.End());
  
  struct stat stbuf;
  EXPECT_EQ(0, fstat(fd(), &stbuf));
  EXPECT_EQ(data.size(), stbuf.st_size);
  
  vector<char> result_file;
  EXPECT_TRUE(utils::ReadFile(path(), &result_file));
  
  vector<char> expected_file;
  expected_file.insert(expected_file.end(),
                       data.begin() + kBlockSize,
                       data.begin() + kBlockSize * 2);
  expected_file.insert(expected_file.end(),
                       data.begin(), data.begin() + kBlockSize);
  expected_file.insert(expected_file.end(),
                       data.begin() + kBlockSize * 2, data.end());
  ExpectVectorsEq(expected_file, result_file);
}

TEST_F(ExtentWriterTest, ZeroPadNullTest) {
  TestZeroPad(true);
}

TEST_F(ExtentWriterTest, ZeroPadFillTest) {
  TestZeroPad(false);
}

void ExtentWriterTest::TestZeroPad(bool aligned_size) {
  vector<Extent> extents;
  Extent extent;
  extent.set_start_block(1);
  extent.set_num_blocks(1);
  extents.push_back(extent);
  extent.set_start_block(0);
  extent.set_num_blocks(1);
  extents.push_back(extent);
  
  vector<char> data(kBlockSize * 2);
  FillWithData(&data);
  
  DirectExtentWriter direct_writer;
  ZeroPadExtentWriter zero_pad_writer(&direct_writer);

  EXPECT_TRUE(zero_pad_writer.Init(fd(), extents, kBlockSize));
  size_t bytes_to_write = data.size();
  const size_t missing_bytes = (aligned_size ? 0 : 9);
  bytes_to_write -= missing_bytes;
  lseek64(fd(), kBlockSize - missing_bytes, SEEK_SET);
  EXPECT_EQ(3, write(fd(), "xxx", 3));
  ASSERT_TRUE(zero_pad_writer.Write(&data[0], bytes_to_write));
  EXPECT_TRUE(zero_pad_writer.End());
  
  struct stat stbuf;
  EXPECT_EQ(0, fstat(fd(), &stbuf));
  EXPECT_EQ(data.size(), stbuf.st_size);
  
  vector<char> result_file;
  EXPECT_TRUE(utils::ReadFile(path(), &result_file));
  
  vector<char> expected_file;
  expected_file.insert(expected_file.end(),
                       data.begin() + kBlockSize,
                       data.begin() + kBlockSize * 2);
  expected_file.insert(expected_file.end(),
                       data.begin(), data.begin() + kBlockSize);
  if (missing_bytes) {
    memset(&expected_file[kBlockSize - missing_bytes], 0, missing_bytes);
  }

  ExpectVectorsEq(expected_file, result_file);
}

TEST_F(ExtentWriterTest, SparseFileTest) {
  vector<Extent> extents;
  Extent extent;
  extent.set_start_block(1);
  extent.set_num_blocks(1);
  extents.push_back(extent);
  extent.set_start_block(kSparseHole);
  extent.set_num_blocks(2);
  extents.push_back(extent);
  extent.set_start_block(0);
  extent.set_num_blocks(1);
  extents.push_back(extent);
  const int block_count = 4;
  const int on_disk_count = 2;

  vector<char> data(17);
  FillWithData(&data);

  DirectExtentWriter direct_writer;
  EXPECT_TRUE(direct_writer.Init(fd(), extents, kBlockSize));
  
  size_t bytes_written = 0;
  while (bytes_written < (block_count * kBlockSize)) {
    size_t bytes_to_write = min(block_count * kBlockSize - bytes_written,
                                data.size());
    EXPECT_TRUE(direct_writer.Write(&data[0], bytes_to_write));
    bytes_written += bytes_to_write;
  }
  EXPECT_TRUE(direct_writer.End());
  
  // check file size, then data inside
  ASSERT_EQ(2 * kBlockSize, FileSize(path()));
  
  vector<char> resultant_data;
  EXPECT_TRUE(utils::ReadFile(path(), &resultant_data));
  
  // Create expected data
  vector<char> expected_data(on_disk_count * kBlockSize);
  vector<char> big(block_count * kBlockSize);
  for (vector<char>::size_type i = 0; i < big.size(); i++) {
    big[i] = data[i % data.size()];
  }
  memcpy(&expected_data[kBlockSize], &big[0], kBlockSize);
  memcpy(&expected_data[0], &big[3 * kBlockSize], kBlockSize);
  ExpectVectorsEq(expected_data, resultant_data);
}

}  // namespace chromeos_update_engine
