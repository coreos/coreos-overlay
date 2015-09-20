// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "files/scoped_file.h"

#include <unistd.h>

#include "base/logging.h"

#include "files/eintr_wrapper.h"

namespace files {

ScopedFD::~ScopedFD() {
  if (fd_ < 0)
    return;
  // It's important to crash here.
  // There are security implications to not closing a file descriptor
  // properly. As file descriptors are "capabilities", keeping them open
  // would make the current process keep access to a resource. Much of
  // Chrome relies on being able to "drop" such access.
  // It's especially problematic on Linux with the setuid sandbox, where
  // a single open directory would bypass the entire security model.
  PCHECK(0 == IGNORE_EINTR(close(fd_)));
}

}  // namespace files
