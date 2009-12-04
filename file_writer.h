// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_FILE_WRITER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_FILE_WRITER_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "chromeos/obsolete_logging.h"
#include "update_engine/utils.h"

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

  virtual int Open(const char* path, int flags, mode_t mode);
  virtual int Write(const void* bytes, size_t count);
  virtual int Close();

  int fd() const { return fd_; }

 private:
  int fd_;
};

class ScopedFileWriterCloser {
 public:
  explicit ScopedFileWriterCloser(FileWriter* writer) : writer_(writer) {}
  ~ScopedFileWriterCloser() {
    int err = writer_->Close();
    if (err)
      LOG(ERROR) << "FileWriter::Close failed: "
                 << utils::ErrnoNumberAsString(-err);
  }
 private:
  FileWriter* writer_;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_FILE_WRITER_H__
