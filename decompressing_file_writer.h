// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_DECOMPRESSING_FILE_WRITER_H__
#define UPDATE_ENGINE_DECOMPRESSING_FILE_WRITER_H__

#include <zlib.h>
#include "base/basictypes.h"
#include "update_engine/file_writer.h"

// GzipDecompressingFileWriter is an implementation of FileWriter. It will
// gzip decompress all data that is passed via Write() onto another FileWriter,
// which is responsible for actually writing the data out. Calls to
// Open and Close are passed through to the other FileWriter.

namespace chromeos_update_engine {

class GzipDecompressingFileWriter : public FileWriter {
 public:
  explicit GzipDecompressingFileWriter(FileWriter* next) : next_(next) {
    memset(&stream_, 0, sizeof(stream_));
    CHECK_EQ(inflateInit2(&stream_, 16 + MAX_WBITS), Z_OK);
  }
  virtual ~GzipDecompressingFileWriter() {
    inflateEnd(&stream_);
  }

  virtual int Open(const char* path, int flags, mode_t mode) {
    return next_->Open(path, flags, mode);
  }

  // Write() calls into zlib to decompress the bytes passed in. It will not
  // necessarily write all the decompressed data associated with this set of
  // passed-in compressed data during this call, however for a valid gzip
  // stream, after the entire stream has been written to this object,
  // the entire decompressed stream will have been written to the
  // underlying FileWriter.
  virtual int Write(const void* bytes, size_t count);

  virtual int Close() {
    return next_->Close();
  }
 private:
  // The FileWriter that we write all uncompressed data to
  FileWriter* next_;

  // The zlib state
  z_stream stream_;

  // This is for an optimization in Write(). We can keep this buffer around
  // in our class to avoid repeated calls to malloc().
  std::vector<char> buffer_;

  DISALLOW_COPY_AND_ASSIGN(GzipDecompressingFileWriter);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_DECOMPRESSING_FILE_WRITER_H__
