// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/subprocess.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include <glog/logging.h>

#include "strings/string_printf.h"
#include "update_engine/utils.h"

using std::shared_ptr;
using std::string;
using std::vector;
using strings::StringPrintf;

namespace chromeos_update_engine {

void Subprocess::GChildExitedCallback(GPid pid, gint status, gpointer data) {
  SubprocessRecord* record = reinterpret_cast<SubprocessRecord*>(data);

  // Make sure we read any remaining process output. Then close the pipe.
  GStdoutWatchCallback(record->gioout, G_IO_IN, &record->stdout);
  int fd = g_io_channel_unix_get_fd(record->gioout);
  g_source_remove(record->gioout_tag);
  g_io_channel_unref(record->gioout);
  close(fd);

  g_spawn_close_pid(pid);
  gint use_status = status;
  if (WIFEXITED(status))
    use_status = WEXITSTATUS(status);

  if (status) {
    LOG(INFO) << "Subprocess status: " << use_status;
  }
  if (!record->stdout.empty()) {
    LOG(INFO) << "Subprocess output:\n" << record->stdout;
  }
  if (record->callback) {
    record->callback(use_status, record->stdout, record->callback_data);
  }
  Get().subprocess_records_.erase(record->tag);
}

void Subprocess::GRedirectStderrToStdout(gpointer user_data) {
  dup2(1, 2);
}

gboolean Subprocess::GStdoutWatchCallback(GIOChannel* source,
                                          GIOCondition condition,
                                          gpointer data) {
  string* stdout = reinterpret_cast<string*>(data);
  char buf[1024];
  gsize bytes_read;
  while (g_io_channel_read_chars(source,
                                 buf,
                                 arraysize(buf),
                                 &bytes_read,
                                 NULL) == G_IO_STATUS_NORMAL &&
         bytes_read > 0) {
    stdout->append(buf, bytes_read);
  }
  return TRUE;  // Keep the callback source. It's freed in GChilExitedCallback.
}

namespace {
void FreeArgv(char** argv) {
  for (int i = 0; argv[i]; i++) {
    free(argv[i]);
    argv[i] = NULL;
  }
}

void FreeArgvInError(char** argv) {
  FreeArgv(argv);
  LOG(ERROR) << "Ran out of memory copying args.";
}

// Note: Caller responsible for free()ing the returned value!
// Will return NULL on failure and free any allocated memory.
char** ArgPointer() {
  const char* keys[] = {"LD_LIBRARY_PATH", "PATH"};
  char** ret = new char*[arraysize(keys) + 1];
  int pointer = 0;
  for (size_t i = 0; i < arraysize(keys); i++) {
    if (getenv(keys[i])) {
      ret[pointer] = strdup(StringPrintf("%s=%s", keys[i],
                                         getenv(keys[i])).c_str());
      if (!ret[pointer]) {
        FreeArgv(ret);
        delete [] ret;
        return NULL;
      }
      ++pointer;
    }
  }
  ret[pointer] = NULL;
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

uint32_t Subprocess::Exec(const vector<string>& cmd,
                          ExecCallback callback,
                          void* p) {
  GPid child_pid;
  std::unique_ptr<char*[]> argv(new char*[cmd.size() + 1]);
  for (unsigned int i = 0; i < cmd.size(); i++) {
    argv[i] = strdup(cmd[i].c_str());
    if (!argv[i]) {
      FreeArgvInError(argv.get());  // NULL in argv[i] terminates argv.
      return 0;
    }
  }
  argv[cmd.size()] = NULL;

  char** argp = ArgPointer();
  if (!argp) {
    FreeArgvInError(argv.get());  // NULL in argv[i] terminates argv.
    return 0;
  }
  ScopedFreeArgPointer argp_free(argp);

  shared_ptr<SubprocessRecord> record(new SubprocessRecord);
  record->callback = callback;
  record->callback_data = p;
  gint stdout_fd = -1;
  GError* error = nullptr;
  bool success = g_spawn_async_with_pipes(
      NULL,  // working directory
      argv.get(),
      argp,
      G_SPAWN_DO_NOT_REAP_CHILD,  // flags
      GRedirectStderrToStdout,  // child setup function
      NULL,  // child setup data pointer
      &child_pid,
      NULL,
      &stdout_fd,
      NULL,
      &error);
  FreeArgv(argv.get());
  if (!success) {
    LOG(ERROR) << "g_spawn_async failed: " << utils::GetAndFreeGError(&error);
    return 0;
  }
  record->tag =
      g_child_watch_add(child_pid, GChildExitedCallback, record.get());
  subprocess_records_[record->tag] = record;

  // Capture the subprocess output.
  record->gioout = g_io_channel_unix_new(stdout_fd);
  g_io_channel_set_encoding(record->gioout, NULL, NULL);
  LOG_IF(WARNING,
         g_io_channel_set_flags(record->gioout, G_IO_FLAG_NONBLOCK, NULL) !=
         G_IO_STATUS_NORMAL) << "Unable to set non-blocking I/O mode.";
  record->gioout_tag = g_io_add_watch(
      record->gioout,
      static_cast<GIOCondition>(G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP),
      GStdoutWatchCallback,
      &record->stdout);
  return record->tag;
}

void Subprocess::CancelExec(uint32_t tag) {
  subprocess_records_[tag]->callback = NULL;
}

bool Subprocess::SynchronousExecFlags(const vector<string>& cmd,
                                      GSpawnFlags flags,
                                      int* return_code,
                                      string* stdout) {
  if (stdout) {
    *stdout = "";
  }
  GError* err = NULL;
  std::unique_ptr<char*[]> argv(new char*[cmd.size() + 1]);
  for (unsigned int i = 0; i < cmd.size(); i++) {
    argv[i] = strdup(cmd[i].c_str());
    if (!argv[i]) {
      FreeArgvInError(argv.get());  // NULL in argv[i] terminates argv.
      return false;
    }
  }
  argv[cmd.size()] = NULL;

  char** argp = ArgPointer();
  if (!argp) {
    FreeArgvInError(argv.get());  // NULL in argv[i] terminates argv.
    return false;
  }
  ScopedFreeArgPointer argp_free(argp);

  char* child_stdout;
  bool success = g_spawn_sync(
      NULL,  // working directory
      argv.get(),
      argp,
      static_cast<GSpawnFlags>(G_SPAWN_STDERR_TO_DEV_NULL |
                               G_SPAWN_SEARCH_PATH | flags),  // flags
      GRedirectStderrToStdout,  // child setup function
      NULL,  // data for child setup function
      &child_stdout,
      NULL,
      return_code,
      &err);
  FreeArgv(argv.get());
  LOG_IF(INFO, err) << utils::GetAndFreeGError(&err);
  if (child_stdout) {
    if (stdout) {
      *stdout = child_stdout;
    } else if (*child_stdout) {
      LOG(INFO) << "Subprocess output:\n" << child_stdout;
    }
    g_free(child_stdout);
  }
  return success;
}

bool Subprocess::SynchronousExec(const std::vector<std::string>& cmd,
                                 int* return_code,
                                 std::string* stdout) {
  return SynchronousExecFlags(cmd,
                              static_cast<GSpawnFlags>(0),
                              return_code,
                              stdout);
}

bool Subprocess::SubprocessInFlight() {
  for (std::map<int, shared_ptr<SubprocessRecord> >::iterator it =
           subprocess_records_.begin();
       it != subprocess_records_.end(); ++it) {
    if (it->second->callback)
      return true;
  }
  return false;
}

Subprocess* Subprocess::subprocess_singleton_ = NULL;

}  // namespace chromeos_update_engine
