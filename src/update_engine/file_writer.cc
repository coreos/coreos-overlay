// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/file_writer.h"
#include <errno.h>

namespace chromeos_update_engine {

int DirectFileWriter::Open() {
  CHECK_EQ(fd_, -1);
  fd_ = open(path_.c_str(), flags_, mode_);
  if (fd_ < 0) {
    PLOG(ERROR) << "Unable to open file " << path_;
    return -errno;
  }
  return 0;
}

bool DirectFileWriter::Write(const void* bytes, size_t count) {
  CHECK_GE(fd_, 0);
  const char* char_bytes = reinterpret_cast<const char*>(bytes);

  size_t bytes_written = 0;
  while (bytes_written < count) {
    ssize_t rc = write(fd_, char_bytes + bytes_written,
                       count - bytes_written);
    if (rc < 0)
      return false;
    bytes_written += rc;
  }
  CHECK_EQ(bytes_written, count);
  return bytes_written == count;
}

int DirectFileWriter::Close() {
  CHECK_GE(fd_, 0);
  int rc = close(fd_);

  // This can be any negative number that's not -1. This way, this FileWriter
  // won't be used again for another file.
  fd_ = -2;

  if (rc < 0)
    return -errno;
  return rc;
}

}  // namespace chromeos_update_engine
