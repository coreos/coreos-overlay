// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_FILE_DESCRIPTOR_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_FILE_DESCRIPTOR_H__

#include <errno.h>
#include <sys/types.h>

#include "base/logging.h"

#include "update_engine/utils.h"

// Abstraction for managing opening, reading, writing and closing of file
// descriptors. This includes an abstract class and one standard implementation
// based on POSIX system calls.
//
// TODO(garnold) this class is modeled after (and augments the functionality of)
// the FileWriter class; ultimately, the latter should be replaced by the former
// throughout the codebase.  A few deviations from the original FileWriter:
//
// * Providing two flavors of Open()
//
// * A FileDescriptor is reusable and can be used to read/write multiple files
//   as long as open/close preconditions are respected.
//
// * Write() returns the number of bytes written: this appears to be more useful
//   for clients, who may wish to retry or otherwise do something useful with
//   the remaining data that was not written.
//
// * Provides a Reset() method, which will force to abandon a currently open
//   file descriptor and allow opening another file, without necessarily
//   properly closing the old one. This may be useful in cases where a "closer"
//   class does not care whether Close() was successful, but may need to reuse
//   the same file descriptor again.

namespace chromeos_update_engine {

// An abstract class defining the file descriptor API.
class FileDescriptor {
 public:
  FileDescriptor() {}
  virtual ~FileDescriptor() {}

  // Opens a file descriptor. The descriptor must be in the closed state prior
  // to this call. Returns true on success, false otherwise. Specific
  // implementations may set errno accordingly.
  virtual bool Open(const char* path, int flags, mode_t mode) = 0;
  virtual bool Open(const char* path, int flags) = 0;

  // Reads from a file descriptor up to a given count. The descriptor must be
  // open prior to this call. Returns the number of bytes read, or -1 on error.
  // Specific implementations may set errno accordingly.
  virtual ssize_t Read(void* buf, size_t count) = 0;

  // Writes to a file descriptor. The descriptor must be open prior to this
  // call. Returns the number of bytes written, or -1 if an error occurred and
  // no bytes were written. Specific implementations may set errno accordingly.
  virtual ssize_t Write(const void* buf, size_t count) = 0;

  // Closes a file descriptor. The descriptor must be open prior to this call.
  // Returns true on success, false otherwise. Specific implementations may set
  // errno accordingly.
  virtual bool Close() = 0;

  // Resets the file descriptor, abandoning a currently open file and returning
  // the descriptor to the closed state.
  virtual void Reset() = 0;

  // Indicates whether or not an implementation sets meaningful errno.
  virtual bool IsSettingErrno() = 0;

  // Indicates whether the descriptor is currently open.
  virtual bool IsOpen() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileDescriptor);
};

// A simple EINTR-immune wrapper implementation around standard system calls.
class EintrSafeFileDescriptor : public FileDescriptor {
 public:
  EintrSafeFileDescriptor() : fd_(-1) {}
  virtual ~EintrSafeFileDescriptor() {};

  // Interface methods.
  virtual bool Open(const char* path, int flags, mode_t mode);
  virtual bool Open(const char* path, int flags);
  virtual ssize_t Read(void* buf, size_t count);
  virtual ssize_t Write(const void* buf, size_t count);
  virtual bool Close();
  virtual void Reset();
  virtual bool IsSettingErrno() {
    return true;
  }
  virtual bool IsOpen() {
    return (fd_ >= 0);
  }

 private:
  int fd_;
};

// A scoped closer for a FileDescriptor object. The destructor of this class
// invokes the Close() method of the given file descriptor, if it's not in the
// closed state already. Note, however, that if Close() fails, this class will
// force a Reset() invocation, which will abandon the current file descriptor.
class ScopedFileDescriptorCloser {
 public:
  explicit ScopedFileDescriptorCloser(FileDescriptor* descriptor)
      : descriptor_(descriptor) {}
  ~ScopedFileDescriptorCloser();
 private:
  FileDescriptor* descriptor_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFileDescriptorCloser);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_FILE_DESCRIPTOR_H__
