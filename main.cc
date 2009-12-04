// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glib.h>

#include "update_engine/subprocess.h"

int main(int argc, char** argv) {
  g_thread_init(NULL);
  chromeos_update_engine::Subprocess::Init();
  return 0;
}
