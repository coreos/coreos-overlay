// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/split_file_writer.h"
#include <algorithm>

using std::min;

namespace chromeos_update_engine {

int SplitFileWriter::Open(const char* path, int flags, mode_t mode) {
  int first_result = first_file_writer_->Open(first_path_,
  first_flags_,
  first_mode_);
  if (first_result < 0) {
    LOG(ERROR) << "Error opening first file " << first_path_;
    return first_result;
  }
  int second_result = second_file_writer_->Open(path, flags, mode);
  if (second_result < 0) {
    LOG(ERROR) << "Error opening second file " << path;
    first_file_writer_->Close();
    return second_result;
  }
  return second_result;
}

namespace {
ssize_t PerformWrite(FileWriter* writer, const void* bytes, size_t count) {
  int rc = writer->Write(bytes, count);
  if (rc < 0) {
    LOG(ERROR) << "Write failed to file.";
    return rc;
  }
  if (rc != static_cast<int>(count)) {
    LOG(ERROR) << "Not all bytes successfully written to file.";
    return -EIO;
  }
  return rc;
}
}

ssize_t SplitFileWriter::Write(const void* bytes, size_t count) {
  const size_t original_count = count;

  // This first block is trying to read the first sizeof(uint64_t)
  // bytes, which are the number of bytes that should be written
  // to the first FileWriter.
  if (bytes_received_ < static_cast<off_t>(sizeof(uint64_t))) {
    // Write more to the initial buffer
    size_t bytes_to_copy = min(static_cast<off_t>(count),
                               static_cast<off_t>(sizeof(first_length_buf_)) -
                               bytes_received_);
    memcpy(&first_length_buf_[bytes_received_], bytes, bytes_to_copy);
    bytes_received_ += bytes_to_copy;
    count -= bytes_to_copy;
    bytes = static_cast<const void*>(
        static_cast<const char*>(bytes) + bytes_to_copy);

    // See if we have all we need
    if (bytes_received_ == sizeof(first_length_buf_)) {
      // Parse first number
      uint64_t big_endian_first_length;
      memcpy(&big_endian_first_length, first_length_buf_,
             sizeof(big_endian_first_length));
      first_length_ = be64toh(big_endian_first_length);
    }
    if (count == 0)
      return original_count;
  }
  CHECK_GE(bytes_received_, static_cast<off_t>(sizeof(uint64_t)));

  // This block of code is writing to the first FileWriter.
  if (bytes_received_ - static_cast<off_t>(sizeof(uint64_t)) < first_length_) {
    // Write to first FileWriter
    size_t bytes_to_write = min(
        first_length_ -
        (bytes_received_ - static_cast<off_t>(sizeof(uint64_t))),
        static_cast<off_t>(count));

    int rc = PerformWrite(first_file_writer_, bytes, bytes_to_write);
    if (rc != static_cast<int>(bytes_to_write))
      return rc;

    bytes_received_ += bytes_to_write;
    count -= bytes_to_write;
    bytes = static_cast<const void*>(
        static_cast<const char*>(bytes) + bytes_to_write);
    if (count == 0)
      return original_count;
  }

  CHECK_GE(static_cast<off_t>(bytes_received_),
           first_length_ + static_cast<off_t>(sizeof(uint64_t)));
  // Write to second FileWriter
  int rc = PerformWrite(second_file_writer_, bytes, count);
  if (rc != static_cast<int>(count))
    return rc;
  return original_count;
}

int SplitFileWriter::Close() {
  int first_result = first_file_writer_->Close();
  if (first_result < 0)
    LOG(ERROR) << "Error Close()ing first file.";
  int second_result = second_file_writer_->Close();
  if (second_result < 0)
    LOG(ERROR) << "Error Close()ing second file.";
  // Return error if either had returned error.
  return second_result < 0 ? second_result : first_result;
}

}  // namespace chromeos_update_engine
