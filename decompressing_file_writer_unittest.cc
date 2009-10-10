// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "update_engine/decompressing_file_writer.h"
#include "update_engine/mock_file_writer.h"
#include "update_engine/test_utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

class GzipDecompressingFileWriterTest : public ::testing::Test { };

TEST(GzipDecompressingFileWriterTest, SimpleTest) {
  MockFileWriter mock_file_writer;
  GzipDecompressingFileWriter decompressing_file_writer(&mock_file_writer);

  // Here is the shell magic to include binary file in C source:
  // hexdump -v -e '" " 12/1 "0x%02x, " "\n"' $FILENAME
  // | sed -e '$s/0x  ,//g' -e 's/^/ /g' | awk
  // 'BEGIN { print "unsigned char file[] = {" } END { print "};" } { print }'

  // uncompressed, contains just 3 bytes: "hi\n"
  unsigned char hi_txt_gz[] = {
    0x1f, 0x8b, 0x08, 0x08, 0x62, 0xf5, 0x8a, 0x4a,
    0x02, 0x03, 0x68, 0x69, 0x2e, 0x74, 0x78, 0x74,
    0x00, 0xcb, 0xc8, 0xe4, 0x02, 0x00, 0x7a, 0x7a,
    0x6f, 0xed, 0x03, 0x00, 0x00, 0x00,
  };
  char hi[] = "hi\n";
  vector<char> hi_vector(hi, hi + strlen(hi));

  const string path("unused");
  ASSERT_EQ(0, decompressing_file_writer.Open(
      path.c_str(), O_CREAT | O_LARGEFILE | O_TRUNC | O_WRONLY, 0644));
  ASSERT_EQ(sizeof(hi_txt_gz),
            decompressing_file_writer.Write(hi_txt_gz, sizeof(hi_txt_gz)));
  ASSERT_EQ(hi_vector.size(), mock_file_writer.bytes().size());
  for (unsigned int i = 0; i < hi_vector.size(); i++) {
    EXPECT_EQ(hi_vector[i], mock_file_writer.bytes()[i]) << "i = " << i;
  }
}

TEST(GzipDecompressingFileWriterTest, IllegalStreamTest) {
  MockFileWriter mock_file_writer;
  GzipDecompressingFileWriter decompressing_file_writer(&mock_file_writer);

  const string path("unused");
  ASSERT_EQ(0, decompressing_file_writer.Open(
      path.c_str(), O_CREAT | O_LARGEFILE | O_TRUNC | O_WRONLY, 0644));
  EXPECT_EQ(0, decompressing_file_writer.Write("\0\0\0\0\0\0\0\0", 8));
  EXPECT_EQ(0, mock_file_writer.bytes().size());
}

TEST(GzipDecompressingFileWriterTest, LargeTest) {
  const string kPath("/tmp/GzipDecompressingFileWriterTest");
  const string kPathgz(kPath + ".gz");
  // First, generate some data, say 10 megs:
  DirectFileWriter uncompressed_file;
  const char* k10bytes = "0123456789";
  const unsigned int k10bytesSize = 10;
  const unsigned int kUncompressedFileSize = strlen(k10bytes) * 1024 * 1024;
  uncompressed_file.Open(kPath.c_str(),
                         O_CREAT | O_LARGEFILE | O_TRUNC | O_WRONLY, 0644);
  for (unsigned int i = 0; i < kUncompressedFileSize / k10bytesSize; i++) {
    ASSERT_EQ(k10bytesSize, uncompressed_file.Write("0123456789", 10));
  }
  uncompressed_file.Close();

  // compress the file
  system((string("cat ") + kPath + " | gzip > " + kPathgz).c_str());

  // Now read the compressed file and put it into a DecompressingFileWriter
  MockFileWriter mock_file_writer;
  GzipDecompressingFileWriter decompressing_file_writer(&mock_file_writer);

  const string path("unused");
  ASSERT_EQ(0, decompressing_file_writer.Open(
      path.c_str(), O_CREAT | O_LARGEFILE | O_TRUNC | O_WRONLY, 0644));

  // Open compressed file for reading:
  int fd_in = open(kPathgz.c_str(), O_LARGEFILE | O_RDONLY, 0);
  ASSERT_GE(fd_in, 0);
  char buf[100];
  int sz;
  while ((sz = read(fd_in, buf, sizeof(buf))) > 0) {
    decompressing_file_writer.Write(buf, sz);
  }
  close(fd_in);
  decompressing_file_writer.Close();

  ASSERT_EQ(kUncompressedFileSize, mock_file_writer.bytes().size());
  for (unsigned int i = 0; i < kUncompressedFileSize; i++) {
    ASSERT_EQ(mock_file_writer.bytes()[i], '0' + (i % 10)) << "i = " << i;
  }
  unlink(kPath.c_str());
  unlink(kPathgz.c_str());
}

}  // namespace chromeos_update_engine
