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
#include <iostream>

#include <base/command_line.h>
#include <base/logging.h>
#include <gflags/gflags.h>
#include <glib.h>

#include "files/file_path.h"
#include "files/scoped_file.h"
#include "strings/string_number_conversions.h"
#include "strings/string_split.h"
#include "update_engine/delta_diff_generator.h"
#include "update_engine/payload_processor.h"
#include "update_engine/payload_signer.h"
#include "update_engine/prefs.h"
#include "update_engine/subprocess.h"
#include "update_engine/terminator.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

DEFINE_string(old_dir, "",
              "Directory where the old partition is loop mounted read-only");
DEFINE_string(new_dir, "",
              "Directory where the new partition is loop mounted read-only");
DEFINE_string(old_image, "", "Path to the old partition");
DEFINE_string(new_image, "", "Path to the new partition");
DEFINE_string(in_file, "",
              "Path to input delta payload file used to hash/sign payloads "
              "and apply delta over old_image (for debugging)");
DEFINE_string(out_file, "", "Path to output delta payload file");
DEFINE_string(out_hash_file, "", "Path to output hash file");
DEFINE_string(out_metadata_hash_file, "", "Path to output metadata hash file");
DEFINE_string(private_key, "", "Path to private key in .pem format");
DEFINE_string(public_key, "", "Path to public key in .pem format");
DEFINE_int32(public_key_version,
             chromeos_update_engine::kSignatureMessageCurrentVersion,
             "Key-check version # of client");
DEFINE_string(prefs_dir, "/tmp/update_engine_prefs",
              "Preferences directory, used with apply_delta");
DEFINE_string(signature_size, "",
              "Raw signature size used for hash calculation. "
              "You may pass in multiple sizes by colon separating them. E.g. "
              "2048:2048:4096 will assume 3 signatures, the first two with "
              "2048 size and the last 4096.");
DEFINE_string(signature_file, "",
              "Raw signature file to sign payload with. To pass multiple "
              "signatures, use a single argument with a colon between paths, "
              "e.g. /path/to/sig:/path/to/next:/path/to/last_sig . Each "
              "signature will be assigned a client version, starting from "
              "kSignatureOriginalVersion.");

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

void ParseSignatureSizes(vector<int>* sizes) {
  LOG_IF(FATAL, FLAGS_signature_size.empty())
      << "Must pass --signature_size to calculate hash for signing.";
  vector<string> strsizes = strings::SplitAndTrim(FLAGS_signature_size, ':');
  for (const string& str : strsizes) {
    int size = 0;
    bool parsing_successful = strings::StringToInt(str, &size);
    LOG_IF(FATAL, !parsing_successful)
        << "Invalid signature size: " << str;
    sizes->push_back(size);
  }
}


void CalculatePayloadHashForSigning() {
  LOG(INFO) << "Calculating payload hash for signing.";
  LOG_IF(FATAL, FLAGS_in_file.empty())
      << "Must pass --in_file to calculate hash for signing.";
  LOG_IF(FATAL, FLAGS_out_hash_file.empty())
      << "Must pass --out_hash_file to calculate hash for signing.";
  vector<int> sizes;
  ParseSignatureSizes(&sizes);

  vector<char> hash;
  bool result = PayloadSigner::HashPayloadForSigning(FLAGS_in_file, sizes,
                                                     &hash);
  CHECK(result);

  result = utils::WriteFile(FLAGS_out_hash_file.c_str(), hash.data(),
                            hash.size());
  CHECK(result);
  LOG(INFO) << "Done calculating payload hash for signing.";
}


void CalculateMetadataHashForSigning() {
  LOG(INFO) << "Calculating metadata hash for signing.";
  LOG_IF(FATAL, FLAGS_in_file.empty())
      << "Must pass --in_file to calculate metadata hash for signing.";
  LOG_IF(FATAL, FLAGS_out_metadata_hash_file.empty())
      << "Must pass --out_metadata_hash_file to calculate metadata hash.";
  vector<int> sizes;
  ParseSignatureSizes(&sizes);

  vector<char> hash;
  bool result = PayloadSigner::HashMetadataForSigning(FLAGS_in_file, &hash);
  CHECK(result);

  result = utils::WriteFile(FLAGS_out_metadata_hash_file.c_str(), hash.data(),
                            hash.size());
  CHECK(result);

  LOG(INFO) << "Done calculating metadata hash for signing.";
}

void SignPayload() {
  LOG(INFO) << "Signing payload.";
  LOG_IF(FATAL, FLAGS_in_file.empty())
      << "Must pass --in_file to sign payload.";
  LOG_IF(FATAL, FLAGS_out_file.empty())
      << "Must pass --out_file to sign payload.";
  LOG_IF(FATAL, FLAGS_signature_file.empty())
      << "Must pass --signature_file to sign payload.";
  vector<vector<char> > signatures;
  vector<string> signature_files = strings::SplitAndTrim(FLAGS_signature_file, ':');
  for (vector<string>::iterator it = signature_files.begin(),
           e = signature_files.end(); it != e; ++it) {
    vector<char> signature;
    CHECK(utils::ReadFile(*it, &signature));
    signatures.push_back(signature);
  }
  uint64_t final_metadata_size;
  CHECK(PayloadSigner::AddSignatureToPayload(
      FLAGS_in_file, signatures, FLAGS_out_file, &final_metadata_size));
  LOG(INFO) << "Done signing payload. Final metadata size = "
            << final_metadata_size;
}

void VerifySignedPayload() {
  LOG(INFO) << "Verifying signed payload.";
  LOG_IF(FATAL, FLAGS_in_file.empty())
      << "Must pass --in_file to verify signed payload.";
  LOG_IF(FATAL, FLAGS_public_key.empty())
      << "Must pass --public_key to verify signed payload.";
  CHECK(PayloadSigner::VerifySignedPayload(FLAGS_in_file, FLAGS_public_key,
                                           FLAGS_public_key_version));
  LOG(INFO) << "Done verifying signed payload.";
}

void ApplyDelta() {
  LOG(INFO) << "Applying delta.";
  LOG_IF(FATAL, FLAGS_old_image.empty())
      << "Must pass --old_image to apply delta.";
  Prefs prefs;
  InstallPlan install_plan;
  LOG(INFO) << "Setting up preferences under: " << FLAGS_prefs_dir;
  LOG_IF(ERROR, !prefs.Init(files::FilePath(FLAGS_prefs_dir)))
      << "Failed to initialize preferences.";
  // Get original checksums
  LOG(INFO) << "Calculating original checksums";
  BlobInfo root_info;
  CHECK(DeltaDiffGenerator::InitializePartitionInfo(FLAGS_old_image,
                                                    &root_info));
  install_plan.rootfs_hash.assign(root_info.hash().begin(),
                                  root_info.hash().end());
  install_plan.install_path = FLAGS_old_image;
  PayloadProcessor performer(&prefs, &install_plan);
  CHECK_EQ(performer.Open(), 0);
  vector<char> buf(1024 * 1024);
  int fd = open(FLAGS_in_file.c_str(), O_RDONLY, 0);
  CHECK_GE(fd, 0);
  files::ScopedFD fd_closer(fd);
  for (off_t offset = 0;; offset += buf.size()) {
    ssize_t bytes_read;
    CHECK(utils::PReadAll(fd, &buf[0], buf.size(), offset, &bytes_read));
    if (bytes_read == 0)
      break;
    CHECK_EQ(performer.Write(&buf[0], bytes_read), bytes_read);
  }
  CHECK_EQ(performer.Close(), 0);
  PayloadProcessor::ResetUpdateProgress(&prefs, false);
  LOG(INFO) << "Done applying delta.";
}

int Main(int argc, char** argv) {
  GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);
  CommandLine::Init(argc, argv);
  Terminator::Init();
  Subprocess::Init();
  logging::InitLogging("delta_generator.log",
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE,
                       logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  if (!FLAGS_signature_size.empty()) {
    bool work_done = false;
    if (!FLAGS_out_hash_file.empty()) {
      CalculatePayloadHashForSigning();
      work_done = true;
    }
    if (!FLAGS_out_metadata_hash_file.empty()) {
      CalculateMetadataHashForSigning();
      work_done = true;
    }
    if (!work_done) {
      LOG(FATAL) << "Neither payload hash file nor metadata hash file supplied";
    }
    return 0;
  }
  if (!FLAGS_signature_file.empty()) {
    SignPayload();
    return 0;
  }
  if (!FLAGS_public_key.empty()) {
    VerifySignedPayload();
    return 0;
  }
  if (!FLAGS_in_file.empty()) {
    ApplyDelta();
    return 0;
  }
  CHECK(!FLAGS_new_image.empty());
  CHECK(!FLAGS_out_file.empty());
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
  uint64_t metadata_size;
  if (!DeltaDiffGenerator::GenerateDeltaUpdateFile(FLAGS_old_dir,
                                                   FLAGS_old_image,
                                                   FLAGS_new_dir,
                                                   FLAGS_new_image,
                                                   FLAGS_out_file,
                                                   FLAGS_private_key,
                                                   &metadata_size)) {
    return 1;
  }
  return 0;
}

}  // namespace {}

}  // namespace chromeos_update_engine

int main(int argc, char** argv) {
  return chromeos_update_engine::Main(argc, argv);
}
