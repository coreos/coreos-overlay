// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "files/scoped_file.h"

#include <unistd.h>

#include <glog/logging.h>

#include "files/eintr_wrapper.h"

namespace files {

namespace internal {

// The eintr wrappers check for -1 while the stdio API defines EOF as the
// return value for errors. It *should* be -1 but lets just make sure.
static_assert(EOF == -1, "fclose's error value is not -1");

void ScopedFILECloser::operator()(FILE* x) const {
  if (x == nullptr)
    return;

  int fd = fileno(x);
  if (IGNORE_EINTR(fclose(x)) == EOF)
    PLOG(ERROR) << "Error while closing file descriptor " << fd;
}

}  // namespace internal

ScopedFD::~ScopedFD() {
  if (fd_ < 0)
    return;

  if (IGNORE_EINTR(close(fd_)) == -1)
    PLOG(ERROR) << "Error while closing file descriptor " << fd_;
}

}  // namespace files
