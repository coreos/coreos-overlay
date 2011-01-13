// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <set>
#include <string>
#include <vector>

#include <base/command_line.h>
#include <base/logging.h>
#include <gflags/gflags.h>
#include <glib.h>

#include "update_engine/delta_diff_generator.h"
#include "update_engine/delta_performer.h"
#include "update_engine/payload_signer.h"
#include "update_engine/prefs.h"
#include "update_engine/subprocess.h"
#include "update_engine/terminator.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

DEFINE_string(old_dir, "",
              "Directory where the old rootfs is loop mounted read-only");
DEFINE_string(new_dir, "",
              "Directory where the new rootfs is loop mounted read-only");
DEFINE_string(old_image, "", "Path to the old rootfs");
DEFINE_string(new_image, "", "Path to the new rootfs");
DEFINE_string(old_kernel, "", "Path to the old kernel partition image");
DEFINE_string(new_kernel, "", "Path to the new kernel partition image");
DEFINE_string(in_file, "",
              "Path to input delta payload file used to hash/sign payloads "
              "and apply delta over old_image (for debugging)");
DEFINE_string(out_file, "", "Path to output delta payload file");
DEFINE_string(out_hash_file, "", "Path to output hash file");
DEFINE_string(private_key, "", "Path to private key in .pem format");
DEFINE_string(prefs_dir, "/tmp/update_engine_prefs",
              "Preferences directory, used with apply_delta");
DEFINE_int32(signature_size, 0, "Raw signature size used for hash calculation");
DEFINE_string(signature_file, "", "Raw signature file to sign payload with");

// This file contains a simple program that takes an old path, a new path,
// and an output file as arguments and the path to an output file and
// generates a delta that can be sent to Chrome OS clients.

using std::set;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {

bool IsDir(const char* path) {
  struct stat stbuf;
  TEST_AND_RETURN_FALSE_ERRNO(lstat(path, &stbuf) == 0);
  return S_ISDIR(stbuf.st_mode);
}

void CalculatePayloadHashForSigning() {
  LOG(INFO) << "Calculating payload hash for signing.";
  LOG_IF(FATAL, FLAGS_in_file.empty())
      << "Must pass --in_file to calculate hash for signing.";
  LOG_IF(FATAL, FLAGS_out_hash_file.empty())
      << "Must pass --out_hash_file to calculate hash for signing.";
  LOG_IF(FATAL, FLAGS_signature_size <= 0)
      << "Must pass --signature_size to calculate hash for signing.";
  vector<char> hash;
  CHECK(PayloadSigner::HashPayloadForSigning(
      FLAGS_in_file, FLAGS_signature_size, &hash));
  CHECK(utils::WriteFile(
      FLAGS_out_hash_file.c_str(), hash.data(), hash.size()));
  LOG(INFO) << "Done calculating payload hash for signing.";
}

void SignPayload() {
  LOG(INFO) << "Signing payload.";
  LOG_IF(FATAL, FLAGS_in_file.empty())
      << "Must pass --in_file to sign payload.";
  LOG_IF(FATAL, FLAGS_out_file.empty())
      << "Must pass --out_file to sign payload.";
  LOG_IF(FATAL, FLAGS_signature_file.empty())
      << "Must pass --signature_file to sign payload.";
  vector<char> signature;
  CHECK(utils::ReadFile(FLAGS_signature_file, &signature));
  CHECK(PayloadSigner::AddSignatureToPayload(
      FLAGS_in_file, signature, FLAGS_out_file));
  LOG(INFO) << "Done signing payload.";
}

void ApplyDelta() {
  LOG(INFO) << "Applying delta.";
  LOG_IF(FATAL, FLAGS_old_image.empty())
      << "Must pass --old_image to apply delta.";
  Prefs prefs;
  LOG(INFO) << "Setting up preferences under: " << FLAGS_prefs_dir;
  LOG_IF(ERROR, !prefs.Init(FilePath(FLAGS_prefs_dir)))
      << "Failed to initialize preferences.";
  // Get original checksums
  LOG(INFO) << "Calculating original checksums";
  PartitionInfo kern_info, root_info;
  CHECK(DeltaDiffGenerator::InitializePartitionInfo(true,  // is_kernel
                                                    FLAGS_old_kernel,
                                                    &kern_info));
  CHECK(DeltaDiffGenerator::InitializePartitionInfo(false,  // is_kernel
                                                    FLAGS_old_image,
                                                    &root_info));
  vector<char> kern_hash(kern_info.hash().begin(),
                         kern_info.hash().end());
  vector<char> root_hash(root_info.hash().begin(),
                         root_info.hash().end());
  DeltaPerformer performer(&prefs);
  performer.set_current_kernel_hash(kern_hash);
  performer.set_current_rootfs_hash(root_hash);
  CHECK_EQ(performer.Open(FLAGS_old_image.c_str(), 0, 0), 0);
  CHECK(performer.OpenKernel(FLAGS_old_kernel.c_str()));
  vector<char> buf(1024 * 1024);
  int fd = open(FLAGS_in_file.c_str(), O_RDONLY, 0);
  CHECK_GE(fd, 0);
  ScopedFdCloser fd_closer(&fd);
  for (off_t offset = 0;; offset += buf.size()) {
    ssize_t bytes_read;
    CHECK(utils::PReadAll(fd, &buf[0], buf.size(), offset, &bytes_read));
    if (bytes_read == 0)
      break;
    CHECK_EQ(performer.Write(&buf[0], bytes_read), bytes_read);
  }
  CHECK_EQ(performer.Close(), 0);
  DeltaPerformer::ResetUpdateProgress(&prefs, false);
  LOG(INFO) << "Done applying delta.";
}

int Main(int argc, char** argv) {
  g_thread_init(NULL);
  google::ParseCommandLineFlags(&argc, &argv, true);
  CommandLine::Init(argc, argv);
  Terminator::Init();
  Subprocess::Init();
  logging::InitLogging("delta_generator.log",
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
  if (FLAGS_signature_size > 0 || !FLAGS_out_hash_file.empty()) {
    CalculatePayloadHashForSigning();
    return 0;
  }
  if (!FLAGS_signature_file.empty()) {
    SignPayload();
    return 0;
  }
  if (!FLAGS_in_file.empty()) {
    ApplyDelta();
    return 0;
  }
  CHECK(!FLAGS_new_image.empty());
  CHECK(!FLAGS_out_file.empty());
  CHECK(!FLAGS_new_kernel.empty());
  if (FLAGS_old_image.empty()) {
    LOG(INFO) << "Generating full update";
  } else {
    LOG(INFO) << "Generating delta update";
    CHECK(!FLAGS_old_dir.empty());
    CHECK(!FLAGS_new_dir.empty());
    if ((!IsDir(FLAGS_old_dir.c_str())) || (!IsDir(FLAGS_new_dir.c_str()))) {
      LOG(FATAL) << "old_dir or new_dir not directory";
    }
  }
  DeltaDiffGenerator::GenerateDeltaUpdateFile(FLAGS_old_dir,
                                              FLAGS_old_image,
                                              FLAGS_new_dir,
                                              FLAGS_new_image,
                                              FLAGS_old_kernel,
                                              FLAGS_new_kernel,
                                              FLAGS_out_file,
                                              FLAGS_private_key);

  return 0;
}

}  // namespace {}

}  // namespace chromeos_update_engine

int main(int argc, char** argv) {
  return chromeos_update_engine::Main(argc, argv);
}
