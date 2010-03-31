// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <set>
#include <string>
#include <gflags/gflags.h>
#include <glib.h>
#include "base/command_line.h"
#include "chromeos/obsolete_logging.h"
#include "update_engine/delta_diff_generator.h"
#include "update_engine/subprocess.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

DEFINE_string(old_dir, "",
              "Directory where the old rootfs is loop mounted read-only");
DEFINE_string(new_dir, "",
              "Directory where the new rootfs is loop mounted read-only");
DEFINE_string(old_image, "", "Path to the old rootfs");
DEFINE_string(new_image, "", "Path to the new rootfs");
DEFINE_string(out_file, "", "Path to output file");

// This file contains a simple program that takes an old path, a new path,
// and an output file as arguments and the path to an output file and
// generates a delta that can be sent to Chrome OS clients.

using std::set;
using std::string;

namespace chromeos_update_engine {

namespace {

bool IsDir(const char* path) {
  struct stat stbuf;
  TEST_AND_RETURN_FALSE_ERRNO(lstat(path, &stbuf) == 0);
  return S_ISDIR(stbuf.st_mode);
}

int Main(int argc, char** argv) {
  g_thread_init(NULL);
  google::ParseCommandLineFlags(&argc, &argv, true);
  CommandLine::Init(argc, argv);
  Subprocess::Init();
  logging::InitLogging("delta_generator.log",
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
  if (FLAGS_old_dir.empty() || FLAGS_new_dir.empty() ||
      FLAGS_old_image.empty() || FLAGS_new_image.empty() ||
      FLAGS_out_file.empty()) {
    LOG(FATAL) << "Missing required argument(s)";
  }
  if ((!IsDir(FLAGS_old_dir.c_str())) || (!IsDir(FLAGS_new_dir.c_str()))) {
    LOG(FATAL) << "old_dir or new_dir not directory";
  }
  
  DeltaDiffGenerator::GenerateDeltaUpdateFile(FLAGS_old_dir,
                                              FLAGS_old_image,
                                              FLAGS_new_dir,
                                              FLAGS_new_image,
                                              FLAGS_out_file);

  return 0;
}

}  // namespace {}

}  // namespace chromeos_update_engine

int main(int argc, char** argv) {
  return chromeos_update_engine::Main(argc, argv);
}
