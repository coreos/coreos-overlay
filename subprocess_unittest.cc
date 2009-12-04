// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <vector>
#include "base/string_util.h"
#include <glib.h>
#include <gtest/gtest.h>
#include "update_engine/subprocess.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

class SubprocessTest : public ::testing::Test {
 protected:
  bool callback_done;
};

namespace {
const int kLocalHttpPort = 8080;

void Callback(int return_code, void *p) {
  EXPECT_EQ(256, return_code);
  GMainLoop* loop = reinterpret_cast<GMainLoop*>(p);
  g_main_loop_quit(loop);
}

gboolean LaunchFalseInMainLoop(gpointer data) {
  vector<string> cmd;
  cmd.push_back("/bin/false");
  Subprocess::Get().Exec(cmd, Callback, data);
  return FALSE;
}
}  // namespace {}

TEST(SubprocessTest, SimpleTest) {
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);
  g_timeout_add(0, &LaunchFalseInMainLoop, loop);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
}

namespace {
void CallbackBad(int return_code, void *p) {
  CHECK(false) << "should never be called.";
}

struct CancelTestData {
  bool spawned;
  GMainLoop *loop;
};

gboolean StartAndCancelInRunLoop(gpointer data) {
  CancelTestData* cancel_test_data = reinterpret_cast<CancelTestData*>(data);
  vector<string> cmd;
  cmd.push_back("./test_http_server");
  uint32 tag = Subprocess::Get().Exec(cmd, CallbackBad, NULL);
  EXPECT_NE(0, tag);
  cancel_test_data->spawned = true;
  printf("spawned\n");
  // Wait for server to be up and running
  for (;;) {
    int status =
        System(StringPrintf("wget -O /dev/null http://127.0.0.1:%d/foo",
                            kLocalHttpPort));
    EXPECT_NE(-1, status) << "system() failed";
    EXPECT_TRUE(WIFEXITED(status))
        << "command failed to run or died abnormally";
    if (0 == WEXITSTATUS(status))
      break;
    usleep(100 * 1000);  // 100 ms
  }
  Subprocess::Get().CancelExec(tag);
  return FALSE;
}
}  // namespace {}

gboolean ExitWhenDone(gpointer data) {
  CancelTestData* cancel_test_data = reinterpret_cast<CancelTestData*>(data);
  if (cancel_test_data->spawned && !Subprocess::Get().SubprocessInFlight()) {
    // tear down the sub process
    printf("tear down time\n");
    int status =
        System(StringPrintf("wget http://127.0.0.1:%d/quitquitquit",
                            kLocalHttpPort));
    EXPECT_NE(-1, status) << "system() failed";
    EXPECT_TRUE(WIFEXITED(status))
        << "command failed to run or died abnormally";
    g_main_loop_quit(cancel_test_data->loop);
    return FALSE;
  }
  return TRUE;
}

TEST(SubprocessTest, CancelTest) {
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);
  CancelTestData cancel_test_data;
  cancel_test_data.spawned = false;
  cancel_test_data.loop = loop;
  g_timeout_add(100, &StartAndCancelInRunLoop, &cancel_test_data);
  g_timeout_add(10, &ExitWhenDone, &cancel_test_data);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
  printf("here\n");
}

}  // namespace chromeos_update_engine
