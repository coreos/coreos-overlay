// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/buffered_file_writer.h"

namespace chromeos_update_engine {

BufferedFileWriter::BufferedFileWriter(FileWriter* next, size_t buffer_size)
    : next_(next),
      buffer_(new char[buffer_size]),
      buffer_size_(buffer_size),
      buffered_bytes_(0) {}

BufferedFileWriter::~BufferedFileWriter() {}

int BufferedFileWriter::Open(const char* path, int flags, mode_t mode) {
  return next_->Open(path, flags, mode);
}

ssize_t BufferedFileWriter::Write(const void* bytes, size_t count) {
  size_t copied_bytes = 0;
  while (copied_bytes < count) {
    // Buffers as many bytes as possible.
    size_t remaining_free = buffer_size_ - buffered_bytes_;
    size_t copy_bytes = count - copied_bytes;
    if (copy_bytes > remaining_free) {
      copy_bytes = remaining_free;
    }
    const char* char_bytes = reinterpret_cast<const char*>(bytes);
    memcpy(&buffer_[buffered_bytes_], char_bytes + copied_bytes, copy_bytes);
    buffered_bytes_ += copy_bytes;
    copied_bytes += copy_bytes;

    // If full, sends the buffer to the next FileWriter.
    if (buffered_bytes_ == buffer_size_) {
      ssize_t rc = WriteBuffer();
      if (rc < 0)
        return rc;
    }
  }
  return count;
}

int BufferedFileWriter::Close() {
  // Flushes the buffer first, then closes the next FileWriter.
  ssize_t rc_write = WriteBuffer();
  int rc_close = next_->Close();
  return (rc_write < 0) ? rc_write : rc_close;
}

ssize_t BufferedFileWriter::WriteBuffer() {
  if (buffered_bytes_ == 0)
    return 0;
  ssize_t rc = next_->Write(&buffer_[0], buffered_bytes_);
  buffered_bytes_ = 0;
  return rc;
}

}  // namespace chromeos_update_engine
