// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FILES_SCOPED_FILE_H_
#define FILES_SCOPED_FILE_H_

#include <stdio.h>

#include <memory>

#include "macros.h"

namespace files {

namespace internal {

// Functor for |ScopedFILE| (below).
struct ScopedFILECloser {
  void operator()(FILE* x) const;
};

}  // namespace internal

// Automatically closes |FILE*|s.
typedef std::unique_ptr<FILE, internal::ScopedFILECloser> ScopedFILE;

// Automatically closes a file descriptor.
class ScopedFD {
 public:
  explicit ScopedFD(int fd) : fd_(fd) {}
  ~ScopedFD();

  int get() const { return fd_; }
  bool is_valid() const { return fd_ >= 0; }

 private:
  int fd_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedFD);
};

}  // namespace files

#endif  // FILES_SCOPED_FILE_H_
