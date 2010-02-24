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
#include "base/basictypes.h"
#include "update_engine/extent_mapper.h"
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
  const off_t kBlockSize = 4096;
  
  vector<Extent> extents;
  
  ASSERT_TRUE(extent_mapper::ExtentsForFileFibmap(kFilename, &extents));
  
  EXPECT_FALSE(extents.empty());
  set<uint64> blocks;
  
  for (vector<Extent>::const_iterator it = extents.begin();
       it != extents.end(); ++it) {
    for (uint64 block = it->start_block();
         block < it->start_block() + it->num_blocks();
         block++) {
      EXPECT_FALSE(utils::SetContainsKey(blocks, block));
      blocks.insert(block);
    }
  }
  
  struct stat stbuf;
  EXPECT_EQ(0, stat(kFilename.c_str(), &stbuf));
  EXPECT_EQ(blocks.size(), (stbuf.st_size + kBlockSize - 1)/kBlockSize);
}

}  // namespace chromeos_update_engine
