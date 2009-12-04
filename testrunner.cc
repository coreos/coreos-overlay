// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// based on pam_google_testrunner.cc

#include <glib.h>
#include <gtest/gtest.h>

#include "update_engine/subprocess.h"

int main(int argc, char **argv) {
  g_thread_init(NULL);
  chromeos_update_engine::Subprocess::Init();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
