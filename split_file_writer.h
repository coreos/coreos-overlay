// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_SPLIT_FILE_WRITER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_SPLIT_FILE_WRITER_H__

#include "update_engine/file_writer.h"

// SplitFileWriter is an implementation of FileWriter suited to our
// full autoupdate format. The first 8 bytes read are assumed to be a
// big-endian number describing how many of the next following bytes
// go to the first FileWriter. After that, the rest of the bytes all
// go to the second FileWriter.

namespace chromeos_update_engine {

class SplitFileWriter : public FileWriter {
 public:
  SplitFileWriter(FileWriter* first_file_writer, FileWriter* second_file_writer)
      : first_file_writer_(first_file_writer),
        first_length_(0),
        first_path_(NULL),
        first_flags_(0),
        first_mode_(0),
        second_file_writer_(second_file_writer),
        bytes_received_(0) {}

  void SetFirstOpenArgs(const char* path, int flags, mode_t mode) {
    first_path_ = path;
    first_flags_ = flags;
    first_mode_ = mode;
  }

  // If both succeed, returns the return value from the second Open() call.
  // On error, both files will be left closed.
  virtual int Open(const char* path, int flags, mode_t mode);

  virtual ssize_t Write(const void* bytes, size_t count);

  virtual int Close();

 private:
  // Data for the first file writer.
  FileWriter* const first_file_writer_;
  off_t first_length_;
  const char* first_path_;
  int first_flags_;
  mode_t first_mode_;

  // The second file writer.
  FileWriter* const second_file_writer_;

  // Bytes written thus far.
  off_t bytes_received_;
  char first_length_buf_[sizeof(uint64_t)];

  DISALLOW_COPY_AND_ASSIGN(SplitFileWriter);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_SPLIT_FILE_WRITER_H__
