// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_BZIP_EXTENT_WRITER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_BZIP_EXTENT_WRITER_H__

#include <vector>
#include <bzlib.h>
#include "update_engine/extent_writer.h"
#include "update_engine/utils.h"

// BzipExtentWriter is a concrete ExtentWriter subclass that bzip-decompresses
// what it's given in Write. It passes the decompressed data to an underlying
// ExtentWriter.

namespace chromeos_update_engine {

class BzipExtentWriter : public ExtentWriter {
 public:
  BzipExtentWriter(ExtentWriter* next) : next_(next) {
    memset(&stream_, 0, sizeof(stream_));
  }
  ~BzipExtentWriter() {}

  bool Init(int fd, const std::vector<Extent>& extents, uint32 block_size);
  bool Write(const void* bytes, size_t count);
  bool EndImpl();

 private:
  ExtentWriter* const next_;  // The underlying ExtentWriter.
  bz_stream stream_;  // the libbz2 stream
  std::vector<char> input_buffer_;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_BZIP_EXTENT_WRITER_H__
