// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
  LOG(INFO) << "started";
  ::g_type_init();
  g_thread_init(NULL);
  dbus_g_thread_init();
  base::AtExitManager exit_manager;
  // TODO(garnold) temporarily cause the unittest binary to exit with status
  // code 2 upon catching a SIGTERM. This will help diagnose why the unittest
  // binary is perceived as failing by the buildbot.  We should revert it to use
  // the default exit status of 1.  Corresponding reverts are necessary in
  // terminator_unittest.cc.
  chromeos_update_engine::Terminator::Init(2);
  chromeos_update_engine::Subprocess::Init();
  LOG(INFO) << "parsing command line arguments";
  CommandLine::Init(argc, argv);
  LOG(INFO) << "initializing gtest";
  ::testing::InitGoogleTest(&argc, argv);
  LOG(INFO) << "running unit tests";
  int test_result = RUN_ALL_TESTS();
  LOG(INFO) << "unittest return value: " << test_result;
  return test_result;
}
