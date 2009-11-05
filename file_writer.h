// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_FILE_WRITER_H__
#define UPDATE_ENGINE_FILE_WRITER_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "base/logging.h"

// FileWriter is a class that is used to (synchronously, for now) write to
// a file. This file is a thin wrapper around open/write/close system calls,
// but provides and interface that can be customized by subclasses that wish
// to filter the data.

namespace chromeos_update_engine {

class FileWriter {
 public:
  virtual ~FileWriter() {}

  // Wrapper around open. Returns 0 on success or -errno on error.
  virtual int Open(const char* path, int flags, mode_t mode) = 0;

  // Wrapper around write. Returns bytes written on success or
  // -errno on error.
  virtual int Write(const void* bytes, size_t count) = 0;

  // Wrapper around close. Returns 0 on success or -errno on error.
  virtual int Close() = 0;
};

// Direct file writer is probably the simplest FileWriter implementation.
// It calls the system calls directly.

class DirectFileWriter : public FileWriter {
 public:
  DirectFileWriter() : fd_(-1) {}
  virtual ~DirectFileWriter() {}

  virtual int Open(const char* path, int flags, mode_t mode) {
    CHECK_EQ(-1, fd_);
    fd_ = open(path, flags, mode);
    if (fd_ < 0)
      return -errno;
    return 0;
  }

  virtual int Write(const void* bytes, size_t count) {
    CHECK_GE(fd_, 0);
    ssize_t rc = write(fd_, bytes, count);
    if (rc < 0)
      return -errno;
    return rc;
  }

  virtual int Close() {
    CHECK_GE(fd_, 0);
    int rc = close(fd_);

    // This can be any negative number that's not -1. This way, this FileWriter
    // won't be used again for another file.
    fd_ = -2;

    if (rc < 0)
      return -errno;
    return rc;
  }

 private:
  int fd_;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_FILE_WRITER_H__
