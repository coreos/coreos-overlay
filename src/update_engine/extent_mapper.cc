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

#include "files/scoped_file.h"
#include "update_engine/graph_types.h"
#include "update_engine/graph_utils.h"
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
  struct stat stbuf;
  int rc = stat(path.c_str(), &stbuf);
  TEST_AND_RETURN_FALSE_ERRNO(rc == 0);
  TEST_AND_RETURN_FALSE(S_ISREG(stbuf.st_mode));

  int fd = open(path.c_str(), O_RDONLY, 0);
  TEST_AND_RETURN_FALSE_ERRNO(fd >= 0);
  files::ScopedFD fd_closer(fd);

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
    unsigned int block32 = i;
    rc = ioctl(fd, FIBMAP, &block32);
    TEST_AND_RETURN_FALSE_ERRNO(rc == 0);
    
    const uint64_t block = (block32 == 0 ? kSparseHole : block32);
    
    graph_utils::AppendBlockToExtents(out, block);
  }
  return true;
}

bool GetFilesystemBlockSize(const std::string& path, uint32_t* out_blocksize) {
  int fd = open(path.c_str(), O_RDONLY, 0);
  TEST_AND_RETURN_FALSE_ERRNO(fd >= 0);
  files::ScopedFD fd_closer(fd);
  int rc = ioctl(fd, FIGETBSZ, out_blocksize);
  TEST_AND_RETURN_FALSE_ERRNO(rc != -1);
  return true;
}

}  // namespace extent_mapper

}  // namespace chromeos_update_engine
