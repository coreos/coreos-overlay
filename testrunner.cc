// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// based on pam_google_testrunner.cc

#include <base/at_exit.h>
#include <base/command_line.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>
#include <glib-object.h>
#include <gtest/gtest.h>

#include "update_engine/subprocess.h"
#include "update_engine/terminator.h"

int main(int argc, char **argv) {
  ::g_type_init();
  g_thread_init(NULL);
  dbus_g_thread_init();
  base::AtExitManager exit_manager;
  chromeos_update_engine::Terminator::Init();
  chromeos_update_engine::Subprocess::Init();
  CommandLine::Init(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  int test_result = RUN_ALL_TESTS();
  LOG(INFO) << "unittest return value: " << test_result;
  return test_result;
}
