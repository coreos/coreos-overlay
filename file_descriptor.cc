// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/file_descriptor.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/posix/eintr_wrapper.h>

namespace chromeos_update_engine {

bool EintrSafeFileDescriptor::Open(const char* path, int flags, mode_t mode) {
  CHECK_EQ(fd_, -1);
  return ((fd_ = HANDLE_EINTR(open(path, flags, mode))) >= 0);
}

bool EintrSafeFileDescriptor::Open(const char* path, int flags) {
  CHECK_EQ(fd_, -1);
  return ((fd_ = HANDLE_EINTR(open(path, flags))) >= 0);
}

ssize_t EintrSafeFileDescriptor::Read(void* buf, size_t count) {
  CHECK_GE(fd_, 0);
  return HANDLE_EINTR(read(fd_, buf, count));
}

ssize_t EintrSafeFileDescriptor::Write(const void* buf, size_t count) {
  CHECK_GE(fd_, 0);

  // Attempt repeated writes, as long as some progress is being made.
  char* char_buf = const_cast<char*>(reinterpret_cast<const char*>(buf));
  ssize_t written = 0;
  while (count > 0) {
    ssize_t ret = HANDLE_EINTR(write(fd_, char_buf, count));

    // Fail on either an error or no progress.
    if (ret <= 0)
      return (written ? written : ret);
    written += ret;
    count -= ret;
    char_buf += ret;
  }
  return written;
}

bool EintrSafeFileDescriptor::Close() {
  CHECK_GE(fd_, 0);
  if (HANDLE_EINTR(close(fd_)))
    return false;
  Reset();
  return true;
}

void EintrSafeFileDescriptor::Reset() {
  fd_ = -1;
}


ScopedFileDescriptorCloser::~ScopedFileDescriptorCloser() {
  if (descriptor_ && descriptor_->IsOpen() && !descriptor_->Close()) {
    const char* err_str = "file closing failed";
    if (descriptor_->IsSettingErrno()) {
      PLOG(ERROR) << err_str;
    } else {
      LOG(ERROR) << err_str;
    }
    // Abandon the current descriptor, forcing it back to a closed state.
    descriptor_->Reset();
  }
}

}  // namespace chromeos_update_engine
