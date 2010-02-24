// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/extent_mapper.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include <linux/fs.h>

#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace extent_mapper {

namespace {
const int kBlockSize = 4096;
}

bool ExtentsForFileFibmap(const std::string& path, std::vector<Extent>* out) {
  CHECK(out);
  // TODO(adlr): verify path is a file
  struct stat stbuf;
  int rc = stat(path.c_str(), &stbuf);
  TEST_AND_RETURN_FALSE_ERRNO(rc == 0);
  TEST_AND_RETURN_FALSE(S_ISREG(stbuf.st_mode));
  
  int fd = open(path.c_str(), O_RDONLY, 0);
  TEST_AND_RETURN_FALSE_ERRNO(fd >= 0);
  ScopedFdCloser fd_closer(&fd);
  
  // Get file size in blocks
  rc = fstat(fd, &stbuf);
  if (rc < 0) {
    perror("fstat");
    return false;
  }
  const int block_count = (stbuf.st_size + kBlockSize - 1) / kBlockSize;
  Extent current;
  current.set_start_block(0);
  current.set_num_blocks(0);

  for (int i = 0; i < block_count; i++) {
    unsigned int block = i;
    rc = ioctl(fd, FIBMAP, &block);
    TEST_AND_RETURN_FALSE_ERRNO(rc == 0);
    
    // Add next block to extents
    if (current.num_blocks() == 0) {
      // We're starting a new extent
      current.set_start_block(block);
      current.set_num_blocks(1);
      continue;
    }
    if ((current.start_block() + current.num_blocks()) == block) {
      // We're continuing the last extent
      current.set_num_blocks(current.num_blocks() + 1);
      continue;
    }
    // We're starting a new extent and keeping the current one
    out->push_back(current);
    current.set_start_block(block);
    current.set_num_blocks(1);
    continue;
  }
  
  if (current.num_blocks() > 0)
    out->push_back(current);

  return true;
}

}  // namespace extent_mapper

}  // namespace chromeos_update_engine
