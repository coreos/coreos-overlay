// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_FILE_WRITER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_FILE_WRITER_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <glog/logging.h>
#include "update_engine/action_processor.h"
#include "update_engine/utils.h"

// FileWriter is a class that is used to (synchronously, for now) write to
// a file. This file is a thin wrapper around open/write/close system calls,
// but provides and interface that can be customized by subclasses that wish
// to filter the data.

namespace chromeos_update_engine {

class FileWriter {
 public:
  FileWriter() {}
  virtual ~FileWriter() {}

  // Wrapper around open. Returns 0 on success or -errno on error.
  virtual int Open() = 0;

  // Wrapper around write. Returns true if all requested bytes
  // were written, or false on any error, reguardless of progress.
  virtual bool Write(const void* bytes, size_t count) = 0;

  // Same as the Write method above but returns a detailed |error| code
  // in addition if the returned value is false. By default this method
  // returns kActionExitDownloadWriteError as the error code, but subclasses
  // can override if they wish to return more specific error codes.
  virtual bool Write(const void* bytes,
                     size_t count,
                     ActionExitCode* error) {
     *error = kActionCodeDownloadWriteError;
     return Write(bytes, count);
  }

  // Wrapper around close. Returns 0 on success or -errno on error.
  virtual int Close() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileWriter);
};

// Direct file writer is probably the simplest FileWriter implementation.
// It calls the system calls directly.

class DirectFileWriter : public FileWriter {
 public:
  DirectFileWriter(
      const std::string path,
      int flags = O_TRUNC | O_WRONLY | O_CREAT | O_LARGEFILE,
      mode_t mode = 0644)
      : path_(path),
        flags_(flags),
        mode_(mode),
        fd_(-1) {}
  DirectFileWriter() = delete;
  virtual ~DirectFileWriter() {}

  virtual int Open();
  virtual bool Write(const void* bytes, size_t count);
  virtual int Close();

  int fd() const { return fd_; }

 private:
  const std::string path_;
  const int flags_;
  const mode_t mode_;
  int fd_;

  DISALLOW_COPY_AND_ASSIGN(DirectFileWriter);
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

  DISALLOW_COPY_AND_ASSIGN(ScopedFileWriterCloser);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_FILE_WRITER_H__
