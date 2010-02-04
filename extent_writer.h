// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_EXTENT_WRITER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_EXTENT_WRITER_H__

#include <vector>
#include "chromeos/obsolete_logging.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

// ExtentWriter is an abstract class which synchronously writes to a given
// file descriptor at the extents given.

namespace chromeos_update_engine {

// When an extent's start block is kSparseHole, that data written for that
// extent will be dropped rather than written to the unerlying fd.
const uint64 kSparseHole = kuint64max;

class ExtentWriter {
 public:
  ExtentWriter() : end_called_(false) {}
  virtual ~ExtentWriter() {
    LOG_IF(ERROR, !end_called_) << "End() not called on ExtentWriter.";
  }

  // Returns true on success.
  virtual bool Init(int fd,
                    const std::vector<Extent>& extents,
                    size_t block_size) = 0;

  // Returns true on success.
  virtual bool Write(const void* bytes, size_t count) = 0;

  // Should be called when all writing is complete. Returns true on success.
  // The fd is not closed. Caller is responsible for closing it.
  bool End() {
    end_called_ = true;
    return EndImpl();
  }
  virtual bool EndImpl() = 0;
 private:
  bool end_called_;
};

// DirectExtentWriter is probably the simplest ExtentWriter implementation.
// It writes the data directly into the extents.

class DirectExtentWriter : public ExtentWriter {
 public:
  DirectExtentWriter()
      : fd_(-1),
        block_size_(0),
        extent_bytes_written_(0),
        next_extent_index_(0) {}
  ~DirectExtentWriter() {}

  bool Init(int fd, const std::vector<Extent>& extents, size_t block_size) {
    fd_ = fd;
    block_size_ = block_size;
    extents_ = extents;
    return true;
  }
  bool Write(const void* bytes, size_t count);
  bool EndImpl() {
    return true;
  }

 private:
  int fd_;
  
  size_t block_size_;
  // Bytes written into next_extent_index_ thus far
  uint64 extent_bytes_written_;
  std::vector<Extent> extents_;
  // The next call to write should correspond to extents_[next_extent_index_]
  std::vector<Extent>::size_type next_extent_index_;
};

// Takes an underlying ExtentWriter to which all operations are delegated.
// When End() is called, ZeroPadExtentWriter ensures that the total number
// of bytes written is a multiple of block_size_. If not, it writes zeros
// to pad as needed.

class ZeroPadExtentWriter : public ExtentWriter {
 public:
  ZeroPadExtentWriter(ExtentWriter* underlying_extent_writer)
      : underlying_extent_writer_(underlying_extent_writer),
        block_size_(0),
        bytes_written_mod_block_size_(0) {}
  ~ZeroPadExtentWriter() {}

  bool Init(int fd, const std::vector<Extent>& extents, size_t block_size) {
    block_size_ = block_size;
    return underlying_extent_writer_->Init(fd, extents, block_size);
  }
  bool Write(const void* bytes, size_t count) {
    if (underlying_extent_writer_->Write(bytes, count)) {
      bytes_written_mod_block_size_ += count;
      bytes_written_mod_block_size_ %= block_size_;
      return true;
    }
    return false;
  }
  bool EndImpl() {
    if (bytes_written_mod_block_size_) {
      const size_t write_size = block_size_ - bytes_written_mod_block_size_;
      std::vector<char> zeros(write_size, 0);
      TEST_AND_RETURN_FALSE(underlying_extent_writer_->Write(&zeros[0],
                                                             write_size));
    }
    return underlying_extent_writer_->End();
  }

 private:
  ExtentWriter* underlying_extent_writer_;  // The underlying ExtentWriter.
  size_t block_size_;
  size_t bytes_written_mod_block_size_;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_EXTENT_WRITER_H__
