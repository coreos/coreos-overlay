// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/subprocess.h"
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

void Subprocess::GChildExitedCallback(GPid pid, gint status, gpointer data) {
  COMPILE_ASSERT(sizeof(guint) == sizeof(uint32_t),
                 guint_uint32_size_mismatch);
  guint* tag = reinterpret_cast<guint*>(data);
  const SubprocessCallbackRecord& record = Get().callback_records_[*tag];
  if (record.callback)
    record.callback(status, record.callback_data);
  g_spawn_close_pid(pid);
  Get().callback_records_.erase(*tag);
  delete tag;
}

namespace {
void FreeArgv(char** argv) {
  for (int i = 0; argv[i]; i++) {
    free(argv[i]);
    argv[i] = NULL;
  }
}

// Note: Caller responsible for free()ing the returned value!
char** ArgPointer() {
  const char* keys[] = {"LD_LIBRARY_PATH", "PATH"};
  char** ret = new char*[arraysize(keys) + 1];
  int pointer = 0;
  for (size_t i = 0; i < arraysize(keys); i++) {
    ret[i] = NULL;
    if (getenv(keys[i])) {
      ret[pointer] = strdup(StringPrintf("%s=%s", keys[i],
                                         getenv(keys[i])).c_str());
      pointer++;
    }
  }
  return ret;
}

class ScopedFreeArgPointer {
 public:
  ScopedFreeArgPointer(char** arr) : arr_(arr) {}
  ~ScopedFreeArgPointer() {
    if (!arr_)
      return;
    for (int i = 0; arr_[i]; i++)
      free(arr_[i]);
    delete[] arr_;
  }
 private:
  char** arr_;
  DISALLOW_COPY_AND_ASSIGN(ScopedFreeArgPointer);
};
}  // namespace {}

uint32_t Subprocess::Exec(const std::vector<std::string>& cmd,
                          ExecCallback callback,
                          void* p) {
  GPid child_pid;
  GError* err;
  scoped_array<char*> argv(new char*[cmd.size() + 1]);
  for (unsigned int i = 0; i < cmd.size(); i++) {
    argv[i] = strdup(cmd[i].c_str());
  }
  argv[cmd.size()] = NULL;

  char** argp = ArgPointer();
  ScopedFreeArgPointer argp_free(argp);

  SubprocessCallbackRecord callback_record;
  callback_record.callback = callback;
  callback_record.callback_data = p;

  bool success = g_spawn_async(NULL,  // working directory
                               argv.get(),
                               argp,
                               G_SPAWN_DO_NOT_REAP_CHILD,  // flags
                               NULL,  // child setup function
                               NULL,  // child setup data pointer
                               &child_pid,
                               &err);
  FreeArgv(argv.get());
  if (!success) {
    LOG(ERROR) << "g_spawn_async failed";
    return 0;
  }
  guint* tag = new guint;
  *tag = g_child_watch_add(child_pid, GChildExitedCallback, tag);
  callback_records_[*tag] = callback_record;
  return *tag;
}

void Subprocess::CancelExec(uint32_t tag) {
  if (callback_records_[tag].callback) {
    callback_records_[tag].callback = NULL;
  }
}

bool Subprocess::SynchronousExec(const std::vector<std::string>& cmd,
                                 int* return_code) {
  GError* err = NULL;
  scoped_array<char*> argv(new char*[cmd.size() + 1]);
  for (unsigned int i = 0; i < cmd.size(); i++) {
    argv[i] = strdup(cmd[i].c_str());
  }
  argv[cmd.size()] = NULL;

  char** argp = ArgPointer();
  ScopedFreeArgPointer argp_free(argp);

  char* child_stdout;
  char* child_stderr;

  bool success = g_spawn_sync(NULL,  // working directory
                              argv.get(),
                              argp,
                              static_cast<GSpawnFlags>(NULL),  // flags
                              NULL,  // child setup function
                              NULL,  // data for child setup function
                              &child_stdout,
                              &child_stderr,
                              return_code,
                              &err);
  FreeArgv(argv.get());
  if (err)
    LOG(INFO) << "err is: " << err->code << ", " << err->message;
  if (child_stdout && strlen(child_stdout))
    LOG(INFO) << "Subprocess stdout:" << child_stdout;
  if (child_stderr && strlen(child_stderr))
    LOG(INFO) << "Subprocess stderr:" << child_stderr;
  return success;
}

Subprocess* Subprocess::subprocess_singleton_ = NULL;

}  // namespace chromeos_update_engine
