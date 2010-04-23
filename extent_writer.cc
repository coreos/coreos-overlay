// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/extent_writer.h"
#include <errno.h>
#include <unistd.h>
#include <algorithm>
#include "update_engine/graph_types.h"
#include "update_engine/utils.h"

using std::min;

namespace chromeos_update_engine {

bool DirectExtentWriter::Write(const void* bytes, size_t count) {
  if (count == 0)
    return true;
  const char* c_bytes = reinterpret_cast<const char*>(bytes);
  size_t bytes_written = 0;
  while (count - bytes_written > 0) {
    TEST_AND_RETURN_FALSE(next_extent_index_ < extents_.size());
    uint64_t bytes_remaining_next_extent =
        extents_[next_extent_index_].num_blocks() * block_size_ -
        extent_bytes_written_;
    CHECK_NE(bytes_remaining_next_extent, 0);
    size_t bytes_to_write =
        static_cast<size_t>(min(static_cast<uint64_t>(count - bytes_written),
                                bytes_remaining_next_extent));
    TEST_AND_RETURN_FALSE(bytes_to_write > 0);

    if (extents_[next_extent_index_].start_block() != kSparseHole) {
      const off64_t offset =
          extents_[next_extent_index_].start_block() * block_size_ +
          extent_bytes_written_;
      TEST_AND_RETURN_FALSE_ERRNO(lseek64(fd_, offset, SEEK_SET) !=
                                  static_cast<off64_t>(-1));
      TEST_AND_RETURN_FALSE(
          utils::WriteAll(fd_, c_bytes + bytes_written, bytes_to_write));
    }
    bytes_written += bytes_to_write;
    extent_bytes_written_ += bytes_to_write;
    if (bytes_remaining_next_extent == bytes_to_write) {
      // We filled this extent
      CHECK_EQ(extent_bytes_written_,
               extents_[next_extent_index_].num_blocks() * block_size_);
      // move to next extent
      extent_bytes_written_ = 0;
      next_extent_index_++;
    }
  }
  return true;
}

}  // namespace chromeos_update_engine
