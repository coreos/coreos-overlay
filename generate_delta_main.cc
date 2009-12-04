// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>
#include <tr1/memory>

#include <glib.h>

#include "chromeos/obsolete_logging.h"
#include "update_engine/subprocess.h"
#include "update_engine/update_metadata.pb.h"

using std::sort;
using std::string;
using std::vector;
using std::tr1::shared_ptr;

// This file contains a simple program that takes an old path, a new path,
// and an output file as arguments and the path to an output file and
// generates a delta that can be sent to Chrome OS clients.

namespace chromeos_update_engine {

int main(int argc, char** argv) {
  g_thread_init(NULL);
  Subprocess::Init();
  if (argc != 4) {
    usage(argv[0]);
  }
  const char* old_dir = argv[1];
  const char* new_dir = argv[2];
  if ((!IsDir(old_dir)) || (!IsDir(new_dir))) {
    usage(argv[0]);
  }
  // TODO(adlr): implement using DeltaDiffGenerator
  return 0;
}