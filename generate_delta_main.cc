// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <set>
#include <string>
#include <glib.h>
#include "chromeos/obsolete_logging.h"
#include "update_engine/delta_diff_generator.h"
#include "update_engine/subprocess.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

// This file contains a simple program that takes an old path, a new path,
// and an output file as arguments and the path to an output file and
// generates a delta that can be sent to Chrome OS clients.

using std::set;
using std::string;

namespace chromeos_update_engine {

namespace {
// These paths should never be delta diffed. They should always be transmitted
// in full in the update.
const char* kNonDiffPaths[] = {
  "/boot/extlinux.conf"
};

void usage(const char* argv0) {
  printf("usage: %s old_dir new_dir out_file\n", argv0);
  exit(1);
}

bool IsDir(const char* path) {
  struct stat stbuf;
  TEST_AND_RETURN_FALSE_ERRNO(lstat(path, &stbuf) == 0);
  return S_ISDIR(stbuf.st_mode);
}

int Main(int argc, char** argv) {
  g_thread_init(NULL);
  Subprocess::Init();
  if (argc != 4) {
    usage(argv[0]);
  }
  logging::InitLogging("",
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
  const char* old_dir = argv[1];
  const char* new_dir = argv[2];
  if ((!IsDir(old_dir)) || (!IsDir(new_dir))) {
    usage(argv[0]);
  }
  
  set<string> non_diff_paths;
  for (size_t i = 0; i < arraysize(kNonDiffPaths); i++)
    non_diff_paths.insert(kNonDiffPaths[i]);
  
  DeltaArchiveManifest* manifest =
      DeltaDiffGenerator::EncodeMetadataToProtoBuffer(new_dir);
  CHECK(manifest);
  CHECK(DeltaDiffGenerator::EncodeDataToDeltaFile(manifest,
                                                  old_dir,
                                                  new_dir,
                                                  argv[3],
                                                  non_diff_paths,
                                                  ""));
  return 0;
}

}  // namespace {}

}  // namespace chromeos_update_engine

int main(int argc, char** argv) {
  return chromeos_update_engine::Main(argc, argv);
}
