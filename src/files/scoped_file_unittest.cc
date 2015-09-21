// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include "files/scoped_file.h"

namespace files {

TEST(ScopedFile, ScopedFDDoesClose) {
  int fds[2];
  char c = 0;
  ASSERT_EQ(0, pipe(fds));
  const int write_end = fds[1];
  ScopedFD read_end_closer(fds[0]);
  {
    ScopedFD write_end_closer(fds[1]);
  }
  // This is the only thread. This file descriptor should no longer be valid.
  int ret = close(write_end);
  EXPECT_EQ(-1, ret);
  EXPECT_EQ(EBADF, errno);
  // Make sure read(2) won't block.
  ASSERT_EQ(0, fcntl(fds[0], F_SETFL, O_NONBLOCK));
  // Reading the pipe should EOF.
  EXPECT_EQ(0, read(fds[0], &c, 1));
}

TEST(ScopedFile, ScopedFILEDoesClose) {
  int fds[2];
  char c = 0;
  ASSERT_EQ(0, pipe(fds));
  const int write_end = fds[1];
  ScopedFD read_end_closer(fds[0]);
  {
    FILE* f = fdopen(fds[1], "w");
    ScopedFILE write_end_closer(f);
  }
  // This is the only thread. This file descriptor should no longer be valid.
  int ret = close(write_end);
  EXPECT_EQ(-1, ret);
  EXPECT_EQ(EBADF, errno);
  // Make sure read(2) won't block.
  ASSERT_EQ(0, fcntl(fds[0], F_SETFL, O_NONBLOCK));
  // Reading the pipe should EOF.
  EXPECT_EQ(0, read(fds[0], &c, 1));
}

}  // namespace files
