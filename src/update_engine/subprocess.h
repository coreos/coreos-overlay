// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_SUBPROCESS_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_SUBPROCESS_H__

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <glib.h>
#include <glog/logging.h>

#include "macros.h"

// The Subprocess class is a singleton. It's used to spawn off a subprocess
// and get notified when the subprocess exits. The result of Exec() can
// be saved and used to cancel the callback request. If you know you won't
// call CancelExec(), you may safely lose the return value from Exec().

namespace chromeos_update_engine {

class Subprocess {
 public:
  typedef void(*ExecCallback)(int return_code,
                              const std::string& output,
                              void *p);

  static void Init() {
    CHECK(!subprocess_singleton_);
    subprocess_singleton_ = new Subprocess;
  }

  // Returns a tag > 0 on success.
  uint32_t Exec(const std::vector<std::string>& cmd,
                ExecCallback callback,
                void* p);

  // Used to cancel the callback. The process will still run to completion.
  void CancelExec(uint32_t tag);

  // Executes a command synchronously. Returns true on success. If |stdout| is
  // non-null, the process output is stored in it, otherwise the output is
  // logged. Note that stderr is redirected to stdout.
  static bool SynchronousExecFlags(const std::vector<std::string>& cmd,
                                   GSpawnFlags flags,
                                   int* return_code,
                                   std::string* stdout);
  static bool SynchronousExec(const std::vector<std::string>& cmd,
                              int* return_code,
                              std::string* stdout);

  // Gets the one instance
  static Subprocess& Get() {
    return *subprocess_singleton_;
  }

  // Returns true iff there is at least one subprocess we're waiting on.
  bool SubprocessInFlight();

 private:
  struct SubprocessRecord {
    SubprocessRecord()
        : tag(0),
          callback(NULL),
          callback_data(NULL),
          gioout(NULL),
          gioout_tag(0) {}
    uint32_t tag;
    ExecCallback callback;
    void* callback_data;
    GIOChannel* gioout;
    guint gioout_tag;
    std::string stdout;
  };

  Subprocess() {}

  // Callback for when any subprocess terminates. This calls the user
  // requested callback.
  static void GChildExitedCallback(GPid pid, gint status, gpointer data);

  // Callback which runs in the child before exec to redirect stderr onto
  // stdout.
  static void GRedirectStderrToStdout(gpointer user_data);

  // Callback which runs whenever there is input available on the subprocess
  // stdout pipe.
  static gboolean GStdoutWatchCallback(GIOChannel* source,
                                       GIOCondition condition,
                                       gpointer data);

  // The global instance.
  static Subprocess* subprocess_singleton_;

  // A map from the asynchronous subprocess tag (see Exec) to the subprocess
  // record structure for all active asynchronous subprocesses.
  std::map<int, std::shared_ptr<SubprocessRecord> > subprocess_records_;

  DISALLOW_COPY_AND_ASSIGN(Subprocess);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_SUBPROCESS_H__
