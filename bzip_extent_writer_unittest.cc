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
#include "update_engine/bzip_extent_writer.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::min;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
const char kPathTemplate[] = "./BzipExtentWriterTest-file.XXXXXX";
const uint32 kBlockSize = 4096;
}

class BzipExtentWriterTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    memcpy(path_, kPathTemplate, sizeof(kPathTemplate));
    fd_ = mkstemp(path_);
    ASSERT_GE(fd_, 0);
  }
  virtual void TearDown() {
    close(fd_);
    LOG(INFO) << "unlink: " << path_;
    unlink(path_);
  }
  int fd() { return fd_; }
  const char* path() { return path_; }
  void WriteAlignedExtents(size_t chunk_size, size_t first_chunk_size);
  void TestZeroPad(bool aligned_size);
 private:
  int fd_;
  char path_[sizeof(kPathTemplate)];
};

TEST_F(BzipExtentWriterTest, SimpleTest) {
  vector<Extent> extents;
  Extent extent;
  extent.set_start_block(0);
  extent.set_num_blocks(1);
  extents.push_back(extent);

  // 'echo test | bzip2 | hexdump' yields:
  const char test_uncompressed[] = "test\n";
  unsigned char test[] = {
    0x42, 0x5a, 0x68, 0x39, 0x31, 0x41, 0x59, 0x26, 0x53, 0x59, 0xcc, 0xc3,
    0x71, 0xd4, 0x00, 0x00, 0x02, 0x41, 0x80, 0x00, 0x10, 0x02, 0x00, 0x0c,
    0x00, 0x20, 0x00, 0x21, 0x9a, 0x68, 0x33, 0x4d, 0x19, 0x97, 0x8b, 0xb9,
    0x22, 0x9c, 0x28, 0x48, 0x66, 0x61, 0xb8, 0xea, 0x00,   
  };
  
  DirectExtentWriter direct_writer;
  BzipExtentWriter bzip_writer(&direct_writer);
  EXPECT_TRUE(bzip_writer.Init(fd(), extents, kBlockSize));
  EXPECT_TRUE(bzip_writer.Write(test, sizeof(test)));
  EXPECT_TRUE(bzip_writer.End());
  
  char buf[sizeof(test_uncompressed) + 1];
  memset(buf, 0, sizeof(buf));
  ssize_t bytes_read = pread(fd(), buf, sizeof(buf) - 1, 0);
  EXPECT_EQ(strlen(test_uncompressed), bytes_read);
  EXPECT_EQ(string(buf), string(test_uncompressed));
}

TEST_F(BzipExtentWriterTest, ChunkedTest) {
  const vector<char>::size_type kDecompressedLength = 2048 * 1024;  // 2 MiB
  const string kDecompressedPath = "BzipExtentWriterTest-file-decompressed";
  const string kCompressedPath = "BzipExtentWriterTest-file-compressed";
  const size_t kChunkSize = 3;
  
  vector<Extent> extents;
  Extent extent;
  extent.set_start_block(0);
  extent.set_num_blocks(kDecompressedLength / kBlockSize + 1);
  extents.push_back(extent);

  vector<char> decompressed_data(kDecompressedLength);
  FillWithData(&decompressed_data);
  
  EXPECT_TRUE(WriteFileVector(kDecompressedPath, decompressed_data));
  
  EXPECT_EQ(0, System(string("cat ") + kDecompressedPath + "|bzip2>" +
                      kCompressedPath));

  vector<char> compressed_data;
  EXPECT_TRUE(utils::ReadFile(kCompressedPath, &compressed_data));
  
  DirectExtentWriter direct_writer;
  BzipExtentWriter bzip_writer(&direct_writer);
  EXPECT_TRUE(bzip_writer.Init(fd(), extents, kBlockSize));

  for (vector<char>::size_type i = 0; i < compressed_data.size();
       i += kChunkSize) {
    size_t this_chunk_size = min(kChunkSize, compressed_data.size() - i);
    EXPECT_TRUE(bzip_writer.Write(&compressed_data[i], this_chunk_size));
  }
  EXPECT_TRUE(bzip_writer.End());
  
  vector<char> output(kDecompressedLength + 1);
  ssize_t bytes_read = pread(fd(), &output[0], output.size(), 0);
  EXPECT_EQ(kDecompressedLength, bytes_read);
  output.resize(kDecompressedLength);
  ExpectVectorsEq(decompressed_data, output);
  
  unlink(kDecompressedPath.c_str());
  unlink(kCompressedPath.c_str());
}

}  // namespace chromeos_update_engine
