// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "macros.h"
#include "update_engine/extent_mapper.h"
#include "update_engine/graph_types.h"
#include "update_engine/utils.h"

using std::set;
using std::string;
using std::vector;

namespace chromeos_update_engine {

class ExtentMapperTest : public ::testing::Test {};

TEST(ExtentMapperTest, RunAsRootSimpleTest) {
  // It's hard to have a concrete test for extent mapping without including
  // a specific filesystem image.
  // In lieu of this, we do a weak test: make sure the extents of the unittest
  // executable are consistent and they match with the size of the file.
  const string kFilename = "/proc/self/exe";
  
  uint32_t block_size = 0;
  EXPECT_TRUE(extent_mapper::GetFilesystemBlockSize(kFilename, &block_size));
  EXPECT_GT(block_size, 0);
    
  vector<Extent> extents;
  
  ASSERT_TRUE(extent_mapper::ExtentsForFileFibmap(kFilename, &extents));
  
  EXPECT_FALSE(extents.empty());
  set<uint64_t> blocks;
  
  for (vector<Extent>::const_iterator it = extents.begin();
       it != extents.end(); ++it) {
    for (uint64_t block = it->start_block();
         block < it->start_block() + it->num_blocks();
         block++) {
      EXPECT_FALSE(blocks.count(block));
      blocks.insert(block);
    }
  }
  
  struct stat stbuf;
  EXPECT_EQ(0, stat(kFilename.c_str(), &stbuf));
  EXPECT_EQ(blocks.size(), (stbuf.st_size + block_size - 1)/block_size);
}

TEST(ExtentMapperTest, RunAsRootSparseFileTest) {
  // Create sparse file with one real block, then two sparse ones, then a real
  // block at the end.
  const char tmp_name_template[] =
      "/tmp/ExtentMapperTest.RunAsRootSparseFileTest.XXXXXX";
  char buf[sizeof(tmp_name_template)];
  strncpy(buf, tmp_name_template, sizeof(buf));
  static_assert(sizeof(buf) > 8, "buf size incorrect");
  ASSERT_EQ('\0', buf[sizeof(buf) - 1]);

  int fd = mkstemp(buf);
  ASSERT_GE(fd, 0);

  uint32_t block_size = 0;
  EXPECT_TRUE(extent_mapper::GetFilesystemBlockSize(buf, &block_size));
  EXPECT_GT(block_size, 0);
  
  EXPECT_EQ(1, pwrite(fd, "x", 1, 0));
  EXPECT_EQ(1, pwrite(fd, "x", 1, 3 * block_size));
  close(fd);
  
  vector<Extent> extents;
  EXPECT_TRUE(extent_mapper::ExtentsForFileFibmap(buf, &extents));
  unlink(buf);
  EXPECT_EQ(3, extents.size());
  EXPECT_EQ(1, extents[0].num_blocks());
  EXPECT_EQ(2, extents[1].num_blocks());
  EXPECT_EQ(1, extents[2].num_blocks());
  EXPECT_NE(kSparseHole, extents[0].start_block());
  EXPECT_EQ(kSparseHole, extents[1].start_block());
  EXPECT_NE(kSparseHole, extents[2].start_block());
  EXPECT_NE(extents[2].start_block(), extents[0].start_block());
}

}  // namespace chromeos_update_engine
