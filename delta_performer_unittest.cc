// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mount.h>
#include <inttypes.h>

#include <algorithm>
#include <string>
#include <vector>

#include <base/file_util.h>
#include <base/memory/scoped_ptr.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <google/protobuf/repeated_field.h>
#include <gtest/gtest.h>

#include "update_engine/delta_diff_generator.h"
#include "update_engine/delta_performer.h"
#include "update_engine/extent_ranges.h"
#include "update_engine/full_update_generator.h"
#include "update_engine/graph_types.h"
#include "update_engine/mock_system_state.h"
#include "update_engine/payload_signer.h"
#include "update_engine/prefs_mock.h"
#include "update_engine/test_utils.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

namespace chromeos_update_engine {

using std::min;
using std::string;
using std::vector;
using testing::_;
using testing::Return;

extern const char* kUnittestPrivateKeyPath;
extern const char* kUnittestPublicKeyPath;
extern const char* kUnittestPrivateKey2Path;
extern const char* kUnittestPublicKey2Path;

static const size_t kBlockSize = 4096;
static const char* kBogusMetadataSignature1 = "awSFIUdUZz2VWFiR+ku0Pj00V7bPQPQFYQSXjEXr3vaw3TE4xHV5CraY3/YrZpBvJ5z4dSBskoeuaO1TNC/S6E05t+yt36tE4Fh79tMnJ/z9fogBDXWgXLEUyG78IEQrYH6/eBsQGT2RJtBgXIXbZ9W+5G9KmGDoPOoiaeNsDuqHiBc/58OFsrxskH8E6vMSBmMGGk82mvgzic7ApcoURbCGey1b3Mwne/hPZ/bb9CIyky8Og9IfFMdL2uAweOIRfjoTeLYZpt+WN65Vu7jJ0cQN8e1y+2yka5112wpRf/LLtPgiAjEZnsoYpLUd7CoVpLRtClp97kN2+tXGNBQqkA==";

static const int kDefaultKernelSize = 4096; // Something small for a test
static const char* kNewDataString = "This is new data.";

namespace {
struct DeltaState {
  string a_img;
  string b_img;
  int image_size;

  string delta_path;
  uint64_t metadata_size;

  string old_kernel;
  vector<char> old_kernel_data;

  string new_kernel;
  vector<char> new_kernel_data;

  // The in-memory copy of delta file.
  vector<char> delta;

  // The mock system state object with which we initialize the
  // delta performer.
  MockSystemState mock_system_state;
};

enum SignatureTest {
  kSignatureNone,  // No payload signing.
  kSignatureGenerator,  // Sign the payload at generation time.
  kSignatureGenerated,  // Sign the payload after it's generated.
  kSignatureGeneratedShell,  // Sign the generated payload through shell cmds.
  kSignatureGeneratedShellBadKey,  // Sign with a bad key through shell cmds.
  kSignatureGeneratedShellRotateCl1,  // Rotate key, test client v1
  kSignatureGeneratedShellRotateCl2,  // Rotate key, test client v2
};

// Different options that determine what we should fill into the
// install_plan.metadata_signature to simulate the contents received in the
// Omaha response.
enum MetadataSignatureTest {
  kEmptyMetadataSignature,
  kInvalidMetadataSignature,
  kValidMetadataSignature,
};

enum OperationHashTest {
  kInvalidOperationData,
  kValidOperationData,
};

} // namespace {}

static void CompareFilesByBlock(const string& a_file, const string& b_file) {
  vector<char> a_data, b_data;
  EXPECT_TRUE(utils::ReadFile(a_file, &a_data)) << "file failed: " << a_file;
  EXPECT_TRUE(utils::ReadFile(b_file, &b_data)) << "file failed: " << b_file;

  EXPECT_EQ(a_data.size(), b_data.size());
  EXPECT_EQ(0, a_data.size() % kBlockSize);
  for (size_t i = 0; i < a_data.size(); i += kBlockSize) {
    EXPECT_EQ(0, i % kBlockSize);
    vector<char> a_sub(&a_data[i], &a_data[i + kBlockSize]);
    vector<char> b_sub(&b_data[i], &b_data[i + kBlockSize]);
    EXPECT_TRUE(a_sub == b_sub) << "Block " << (i/kBlockSize) << " differs";
  }
}

static bool WriteSparseFile(const string& path, off_t size) {
  int fd = open(path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
  TEST_AND_RETURN_FALSE_ERRNO(fd >= 0);
  ScopedFdCloser fd_closer(&fd);
  off_t rc = lseek(fd, size + 1, SEEK_SET);
  TEST_AND_RETURN_FALSE_ERRNO(rc != static_cast<off_t>(-1));
  int return_code = ftruncate(fd, size);
  TEST_AND_RETURN_FALSE_ERRNO(return_code == 0);
  return true;
}

static size_t GetSignatureSize(const string& private_key_path) {
  const vector<char> data(1, 'x');
  vector<char> hash;
  EXPECT_TRUE(OmahaHashCalculator::RawHashOfData(data, &hash));
  vector<char> signature;
  EXPECT_TRUE(PayloadSigner::SignHash(hash,
                                      private_key_path,
                                      &signature));
  return signature.size();
}

static void SignGeneratedPayload(const string& payload_path,
                                 uint64_t* out_metadata_size) {
  int signature_size = GetSignatureSize(kUnittestPrivateKeyPath);
  vector<char> hash;
  ASSERT_TRUE(PayloadSigner::HashPayloadForSigning(
      payload_path,
      vector<int>(1, signature_size),
      &hash));
  vector<char> signature;
  ASSERT_TRUE(PayloadSigner::SignHash(hash,
                                      kUnittestPrivateKeyPath,
                                      &signature));
  ASSERT_TRUE(PayloadSigner::AddSignatureToPayload(
      payload_path,
      vector<vector<char> >(1, signature),
      payload_path,
      out_metadata_size));
  EXPECT_TRUE(PayloadSigner::VerifySignedPayload(
      payload_path,
      kUnittestPublicKeyPath,
      kSignatureMessageOriginalVersion));
}

static void SignGeneratedShellPayload(SignatureTest signature_test,
                                      const string& payload_path) {
  string private_key_path = kUnittestPrivateKeyPath;
  if (signature_test == kSignatureGeneratedShellBadKey) {
    ASSERT_TRUE(utils::MakeTempFile("/tmp/key.XXXXXX",
                                    &private_key_path,
                                    NULL));
  } else {
    ASSERT_TRUE(signature_test == kSignatureGeneratedShell ||
                signature_test == kSignatureGeneratedShellRotateCl1 ||
                signature_test == kSignatureGeneratedShellRotateCl2);
  }
  ScopedPathUnlinker key_unlinker(private_key_path);
  key_unlinker.set_should_remove(signature_test ==
                                 kSignatureGeneratedShellBadKey);
  // Generates a new private key that will not match the public key.
  if (signature_test == kSignatureGeneratedShellBadKey) {
    LOG(INFO) << "Generating a mismatched private key.";
    ASSERT_EQ(0,
              System(StringPrintf(
                  "openssl genrsa -out %s 2048",
                  private_key_path.c_str())));
  }
  int signature_size = GetSignatureSize(private_key_path);
  string hash_file;
  ASSERT_TRUE(utils::MakeTempFile("/tmp/hash.XXXXXX", &hash_file, NULL));
  ScopedPathUnlinker hash_unlinker(hash_file);
  string signature_size_string;
  if (signature_test == kSignatureGeneratedShellRotateCl1 ||
      signature_test == kSignatureGeneratedShellRotateCl2)
    signature_size_string = StringPrintf("%d:%d",
                                         signature_size, signature_size);
  else
    signature_size_string = StringPrintf("%d", signature_size);
  ASSERT_EQ(0,
            System(StringPrintf(
                "./delta_generator -in_file %s -signature_size %s "
                "-out_hash_file %s",
                payload_path.c_str(),
                signature_size_string.c_str(),
                hash_file.c_str())));

  // Pad the hash
  vector<char> hash;
  ASSERT_TRUE(utils::ReadFile(hash_file, &hash));
  ASSERT_TRUE(PayloadSigner::PadRSA2048SHA256Hash(&hash));
  ASSERT_TRUE(WriteFileVector(hash_file, hash));

  string sig_file;
  ASSERT_TRUE(utils::MakeTempFile("/tmp/signature.XXXXXX", &sig_file, NULL));
  ScopedPathUnlinker sig_unlinker(sig_file);
  ASSERT_EQ(0,
            System(StringPrintf(
                "openssl rsautl -raw -sign -inkey %s -in %s -out %s",
                private_key_path.c_str(),
                hash_file.c_str(),
                sig_file.c_str())));
  string sig_file2;
  ASSERT_TRUE(utils::MakeTempFile("/tmp/signature.XXXXXX", &sig_file2, NULL));
  ScopedPathUnlinker sig2_unlinker(sig_file2);
  if (signature_test == kSignatureGeneratedShellRotateCl1 ||
      signature_test == kSignatureGeneratedShellRotateCl2) {
    ASSERT_EQ(0,
              System(StringPrintf(
                  "openssl rsautl -raw -sign -inkey %s -in %s -out %s",
                  kUnittestPrivateKey2Path,
                  hash_file.c_str(),
                  sig_file2.c_str())));
    // Append second sig file to first path
    sig_file += ":" + sig_file2;
  }

  ASSERT_EQ(0,
            System(StringPrintf(
                "./delta_generator -in_file %s -signature_file %s "
                "-out_file %s",
                payload_path.c_str(),
                sig_file.c_str(),
                payload_path.c_str())));
  int verify_result =
      System(StringPrintf(
          "./delta_generator -in_file %s -public_key %s -public_key_version %d",
          payload_path.c_str(),
          signature_test == kSignatureGeneratedShellRotateCl2 ?
          kUnittestPublicKey2Path : kUnittestPublicKeyPath,
          signature_test == kSignatureGeneratedShellRotateCl2 ? 2 : 1));
  if (signature_test == kSignatureGeneratedShellBadKey) {
    ASSERT_NE(0, verify_result);
  } else {
    ASSERT_EQ(0, verify_result);
  }
}

static void GenerateDeltaFile(bool full_kernel,
                              bool full_rootfs,
                              bool noop,
                              SignatureTest signature_test,
                              DeltaState *state) {
  EXPECT_TRUE(utils::MakeTempFile("/tmp/a_img.XXXXXX", &state->a_img, NULL));
  EXPECT_TRUE(utils::MakeTempFile("/tmp/b_img.XXXXXX", &state->b_img, NULL));
  CreateExtImageAtPath(state->a_img, NULL);

  state->image_size = static_cast<int>(utils::FileSize(state->a_img));

  // Extend the "partitions" holding the file system a bit.
  EXPECT_EQ(0, System(base::StringPrintf(
      "dd if=/dev/zero of=%s seek=%d bs=1 count=1",
      state->a_img.c_str(),
      state->image_size + 1024 * 1024 - 1)));
  EXPECT_EQ(state->image_size + 1024 * 1024, utils::FileSize(state->a_img));

  // Make some changes to the A image.
  {
    string a_mnt;
    ScopedLoopMounter b_mounter(state->a_img, &a_mnt, 0);

    EXPECT_TRUE(utils::WriteFile(StringPrintf("%s/hardtocompress",
                                              a_mnt.c_str()).c_str(),
                                 reinterpret_cast<const char*>(kRandomString),
                                 sizeof(kRandomString) - 1));
    // Write 1 MiB of 0xff to try to catch the case where writing a bsdiff
    // patch fails to zero out the final block.
    vector<char> ones(1024 * 1024, 0xff);
    EXPECT_TRUE(utils::WriteFile(StringPrintf("%s/ones",
                                              a_mnt.c_str()).c_str(),
                                 &ones[0],
                                 ones.size()));
  }

  if (noop) {
    EXPECT_TRUE(file_util::CopyFile(FilePath(state->a_img),
                                    FilePath(state->b_img)));
  } else {
    CreateExtImageAtPath(state->b_img, NULL);
    EXPECT_EQ(0, System(base::StringPrintf(
        "dd if=/dev/zero of=%s seek=%d bs=1 count=1",
        state->b_img.c_str(),
        state->image_size + 1024 * 1024 - 1)));
    EXPECT_EQ(state->image_size + 1024 * 1024, utils::FileSize(state->b_img));

    // Make some changes to the B image.
    string b_mnt;
    ScopedLoopMounter b_mounter(state->b_img, &b_mnt, 0);

    EXPECT_EQ(0, system(StringPrintf("cp %s/hello %s/hello2", b_mnt.c_str(),
                                     b_mnt.c_str()).c_str()));
    EXPECT_EQ(0, system(StringPrintf("rm %s/hello", b_mnt.c_str()).c_str()));
    EXPECT_EQ(0, system(StringPrintf("mv %s/hello2 %s/hello", b_mnt.c_str(),
                                     b_mnt.c_str()).c_str()));
    EXPECT_EQ(0, system(StringPrintf("echo foo > %s/foo",
                                     b_mnt.c_str()).c_str()));
    EXPECT_EQ(0, system(StringPrintf("touch %s/emptyfile",
                                     b_mnt.c_str()).c_str()));
    EXPECT_TRUE(WriteSparseFile(StringPrintf("%s/fullsparse", b_mnt.c_str()),
                                1024 * 1024));
    EXPECT_EQ(0, system(StringPrintf("dd if=/dev/zero of=%s/partsparese bs=1 "
                                     "seek=4096 count=1",
                                     b_mnt.c_str()).c_str()));
    EXPECT_EQ(0, system(StringPrintf("cp %s/srchardlink0 %s/tmp && "
                                     "mv %s/tmp %s/srchardlink1",
                                     b_mnt.c_str(), b_mnt.c_str(),
                                     b_mnt.c_str(), b_mnt.c_str()).c_str()));
    EXPECT_EQ(0, system(StringPrintf("rm %s/boguslink && "
                                     "echo foobar > %s/boguslink",
                                     b_mnt.c_str(), b_mnt.c_str()).c_str()));
    EXPECT_TRUE(utils::WriteFile(StringPrintf("%s/hardtocompress",
                                              b_mnt.c_str()).c_str(),
                                 reinterpret_cast<const char*>(kRandomString),
                                 sizeof(kRandomString)));
  }

  string old_kernel;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/old_kernel.XXXXXX",
                                  &state->old_kernel,
                                  NULL));

  string new_kernel;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/new_kernel.XXXXXX",
                                  &state->new_kernel,
                                  NULL));

  state->old_kernel_data.resize(kDefaultKernelSize);
  state->new_kernel_data.resize(state->old_kernel_data.size());
  FillWithData(&state->old_kernel_data);
  FillWithData(&state->new_kernel_data);

  // change the new kernel data
  strcpy(&state->new_kernel_data[0], kNewDataString);

  if (noop) {
    state->old_kernel_data = state->new_kernel_data;
  }

  // Write kernels to disk
  EXPECT_TRUE(utils::WriteFile(state->old_kernel.c_str(),
                               &state->old_kernel_data[0],
                               state->old_kernel_data.size()));
  EXPECT_TRUE(utils::WriteFile(state->new_kernel.c_str(),
                               &state->new_kernel_data[0],
                               state->new_kernel_data.size()));

  EXPECT_TRUE(utils::MakeTempFile("/tmp/delta.XXXXXX",
                                  &state->delta_path,
                                  NULL));
  LOG(INFO) << "delta path: " << state->delta_path;
  {
    string a_mnt, b_mnt;
    ScopedLoopMounter a_mounter(state->a_img, &a_mnt, MS_RDONLY);
    ScopedLoopMounter b_mounter(state->b_img, &b_mnt, MS_RDONLY);
    const string private_key =
        signature_test == kSignatureGenerator ? kUnittestPrivateKeyPath : "";
    EXPECT_TRUE(
        DeltaDiffGenerator::GenerateDeltaUpdateFile(
            full_rootfs ? "" : a_mnt,
            full_rootfs ? "" : state->a_img,
            b_mnt,
            state->b_img,
            full_kernel ? "" : state->old_kernel,
            state->new_kernel,
            state->delta_path,
            private_key,
            &state->metadata_size));
  }

  if (signature_test == kSignatureGenerated) {
    // Generate the signed payload and update the metadata size in state to
    // reflect the new size after adding the signature operation to the
    // manifest.
    SignGeneratedPayload(state->delta_path, &state->metadata_size);
  } else if (signature_test == kSignatureGeneratedShell ||
             signature_test == kSignatureGeneratedShellBadKey ||
             signature_test == kSignatureGeneratedShellRotateCl1 ||
             signature_test == kSignatureGeneratedShellRotateCl2) {
    SignGeneratedShellPayload(signature_test, state->delta_path);
  }
}

static void ApplyDeltaFile(bool full_kernel, bool full_rootfs, bool noop,
                           SignatureTest signature_test, DeltaState* state,
                           bool hash_checks_mandatory,
                           OperationHashTest op_hash_test,
                           DeltaPerformer** performer) {
  // Check the metadata.
  {
    DeltaArchiveManifest manifest;
    EXPECT_TRUE(PayloadSigner::LoadPayload(state->delta_path,
                                           &state->delta,
                                           &manifest,
                                           &state->metadata_size));
    LOG(INFO) << "Metadata size: " << state->metadata_size;

    if (signature_test == kSignatureNone) {
      EXPECT_FALSE(manifest.has_signatures_offset());
      EXPECT_FALSE(manifest.has_signatures_size());
    } else {
      EXPECT_TRUE(manifest.has_signatures_offset());
      EXPECT_TRUE(manifest.has_signatures_size());
      Signatures sigs_message;
      EXPECT_TRUE(sigs_message.ParseFromArray(
          &state->delta[state->metadata_size + manifest.signatures_offset()],
          manifest.signatures_size()));
      if (signature_test == kSignatureGeneratedShellRotateCl1 ||
          signature_test == kSignatureGeneratedShellRotateCl2)
        EXPECT_EQ(2, sigs_message.signatures_size());
      else
        EXPECT_EQ(1, sigs_message.signatures_size());
      const Signatures_Signature& signature = sigs_message.signatures(0);
      EXPECT_EQ(1, signature.version());

      uint64_t expected_sig_data_length = 0;
      vector<string> key_paths (1, kUnittestPrivateKeyPath);
      if (signature_test == kSignatureGeneratedShellRotateCl1 ||
          signature_test == kSignatureGeneratedShellRotateCl2) {
        key_paths.push_back(kUnittestPrivateKey2Path);
      }
      EXPECT_TRUE(PayloadSigner::SignatureBlobLength(
          key_paths,
          &expected_sig_data_length));
      EXPECT_EQ(expected_sig_data_length, manifest.signatures_size());
      EXPECT_FALSE(signature.data().empty());
    }

    if (noop) {
      EXPECT_EQ(1, manifest.install_operations_size());
      EXPECT_EQ(1, manifest.kernel_install_operations_size());
    }

    if (full_kernel) {
      EXPECT_FALSE(manifest.has_old_kernel_info());
    } else {
      EXPECT_EQ(state->old_kernel_data.size(),
                manifest.old_kernel_info().size());
      EXPECT_FALSE(manifest.old_kernel_info().hash().empty());
    }

    if (full_rootfs) {
      EXPECT_FALSE(manifest.has_old_rootfs_info());
    } else {
      EXPECT_EQ(state->image_size, manifest.old_rootfs_info().size());
      EXPECT_FALSE(manifest.old_rootfs_info().hash().empty());
    }

    EXPECT_EQ(state->new_kernel_data.size(), manifest.new_kernel_info().size());
    EXPECT_EQ(state->image_size, manifest.new_rootfs_info().size());

    EXPECT_FALSE(manifest.new_kernel_info().hash().empty());
    EXPECT_FALSE(manifest.new_rootfs_info().hash().empty());
  }

  PrefsMock prefs;
  EXPECT_CALL(prefs, SetInt64(kPrefsManifestMetadataSize,
                              state->metadata_size)).WillOnce(Return(true));
  EXPECT_CALL(prefs, SetInt64(kPrefsUpdateStateNextOperation, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(prefs, GetInt64(kPrefsUpdateStateNextOperation, _))
      .WillOnce(Return(false));
  EXPECT_CALL(prefs, SetInt64(kPrefsUpdateStateNextDataOffset, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(prefs, SetString(kPrefsUpdateStateSHA256Context, _))
      .WillRepeatedly(Return(true));
  if (op_hash_test == kValidOperationData && signature_test != kSignatureNone) {
    EXPECT_CALL(prefs, SetString(kPrefsUpdateStateSignedSHA256Context, _))
        .WillOnce(Return(true));
    EXPECT_CALL(prefs, SetString(kPrefsUpdateStateSignatureBlob, _))
        .WillOnce(Return(true));
  }

  // Update the A image in place.
  InstallPlan install_plan;
  install_plan.hash_checks_mandatory = hash_checks_mandatory;
  install_plan.metadata_size = state->metadata_size;
  LOG(INFO) << "Setting payload metadata size in Omaha  = "
            << state->metadata_size;
  ASSERT_TRUE(PayloadSigner::GetMetadataSignature(
      &state->delta[0],
      state->metadata_size,
      kUnittestPrivateKeyPath,
      &install_plan.metadata_signature));
  EXPECT_FALSE(install_plan.metadata_signature.empty());

  *performer = new DeltaPerformer(&prefs,
                                  &state->mock_system_state,
                                  &install_plan);
  EXPECT_TRUE(utils::FileExists(kUnittestPublicKeyPath));
  (*performer)->set_public_key_path(kUnittestPublicKeyPath);

  EXPECT_EQ(state->image_size,
            OmahaHashCalculator::RawHashOfFile(state->a_img,
                                               state->image_size,
                                               &install_plan.rootfs_hash));
  EXPECT_TRUE(OmahaHashCalculator::RawHashOfData(state->old_kernel_data,
                                                 &install_plan.kernel_hash));

  EXPECT_EQ(0, (*performer)->Open(state->a_img.c_str(), 0, 0));
  EXPECT_TRUE((*performer)->OpenKernel(state->old_kernel.c_str()));

  ActionExitCode expected_error, actual_error;
  bool continue_writing;
  switch(op_hash_test) {
    case kInvalidOperationData: {
      // Muck with some random offset post the metadata size so that
      // some operation hash will result in a mismatch.
      int some_offset = state->metadata_size + 300;
      LOG(INFO) << "Tampered value at offset: " << some_offset;
      state->delta[some_offset]++;
      expected_error = kActionCodeDownloadOperationHashMismatch;
      continue_writing = false;
      break;
    }

    case kValidOperationData:
    default:
      // no change.
      expected_error = kActionCodeSuccess;
      continue_writing = true;
      break;
  }

  // Write at some number of bytes per operation. Arbitrarily chose 5.
  const size_t kBytesPerWrite = 5;
  for (size_t i = 0; i < state->delta.size(); i += kBytesPerWrite) {
    size_t count = min(state->delta.size() - i, kBytesPerWrite);
    bool write_succeeded = ((*performer)->Write(&state->delta[i],
                                                count,
                                                &actual_error));
    // Normally write_succeeded should be true every time and
    // actual_error should be kActionCodeSuccess. If so, continue the loop.
    // But if we seeded an operation hash error above, then write_succeeded
    // will be false. The failure may happen at any operation n. So, all
    // Writes until n-1 should succeed and the nth operation will fail with
    // actual_error. In this case, we should bail out of the loop because
    // we cannot proceed applying the delta.
    if (!write_succeeded) {
      LOG(INFO) << "Write failed. Checking if it failed with expected error";
      EXPECT_EQ(expected_error, actual_error);
      if (!continue_writing) {
        LOG(INFO) << "Cannot continue writing. Bailing out.";
        break;
      }
    }

    EXPECT_EQ(kActionCodeSuccess, actual_error);
  }

  // If we had continued all the way through, Close should succeed.
  // Otherwise, it should fail. Check appropriately.
  bool close_result = (*performer)->Close();
  if (continue_writing)
    EXPECT_EQ(0, close_result);
  else
    EXPECT_LE(0, close_result);
}

void VerifyPayloadResult(DeltaPerformer* performer,
                         DeltaState* state,
                         ActionExitCode expected_result) {
  if (!performer) {
    EXPECT_TRUE(!"Skipping payload verification since performer is NULL.");
    return;
  }

  int expected_times = (expected_result == kActionCodeSuccess) ? 1 : 0;
  EXPECT_CALL(*(state->mock_system_state.mock_payload_state()),
              DownloadComplete()).Times(expected_times);

  LOG(INFO) << "Verifying payload for expected result "
            << expected_result;
  EXPECT_EQ(expected_result, performer->VerifyPayload(
      OmahaHashCalculator::OmahaHashOfData(state->delta),
      state->delta.size()));
  LOG(INFO) << "Verified payload.";

  if (expected_result != kActionCodeSuccess) {
    // no need to verify new partition if VerifyPayload failed.
    return;
  }

  CompareFilesByBlock(state->old_kernel, state->new_kernel);
  CompareFilesByBlock(state->a_img, state->b_img);

  vector<char> updated_kernel_partition;
  EXPECT_TRUE(utils::ReadFile(state->old_kernel, &updated_kernel_partition));
  EXPECT_EQ(0, strncmp(&updated_kernel_partition[0], kNewDataString,
                       strlen(kNewDataString)));

  uint64_t new_kernel_size;
  vector<char> new_kernel_hash;
  uint64_t new_rootfs_size;
  vector<char> new_rootfs_hash;
  EXPECT_TRUE(performer->GetNewPartitionInfo(&new_kernel_size,
                                            &new_kernel_hash,
                                            &new_rootfs_size,
                                            &new_rootfs_hash));
  EXPECT_EQ(kDefaultKernelSize, new_kernel_size);
  vector<char> expected_new_kernel_hash;
  EXPECT_TRUE(OmahaHashCalculator::RawHashOfData(state->new_kernel_data,
                                                 &expected_new_kernel_hash));
  EXPECT_TRUE(expected_new_kernel_hash == new_kernel_hash);
  EXPECT_EQ(state->image_size, new_rootfs_size);
  vector<char> expected_new_rootfs_hash;
  EXPECT_EQ(state->image_size,
            OmahaHashCalculator::RawHashOfFile(state->b_img,
                                               state->image_size,
                                               &expected_new_rootfs_hash));
  EXPECT_TRUE(expected_new_rootfs_hash == new_rootfs_hash);
}

void VerifyPayload(DeltaPerformer* performer,
                   DeltaState* state,
                   SignatureTest signature_test) {
  ActionExitCode expected_result = kActionCodeSuccess;
  switch (signature_test) {
    case kSignatureNone:
      expected_result = kActionCodeSignedDeltaPayloadExpectedError;
      break;
    case kSignatureGeneratedShellBadKey:
      expected_result = kActionCodeDownloadPayloadPubKeyVerificationError;
      break;
    default: break;  // appease gcc
  }

  VerifyPayloadResult(performer, state, expected_result);
}

void DoSmallImageTest(bool full_kernel, bool full_rootfs, bool noop,
                      SignatureTest signature_test,
                      bool hash_checks_mandatory) {
  DeltaState state;
  DeltaPerformer *performer;
  GenerateDeltaFile(full_kernel, full_rootfs, noop, signature_test, &state);
  ScopedPathUnlinker a_img_unlinker(state.a_img);
  ScopedPathUnlinker b_img_unlinker(state.b_img);
  ScopedPathUnlinker delta_unlinker(state.delta_path);
  ScopedPathUnlinker old_kernel_unlinker(state.old_kernel);
  ScopedPathUnlinker new_kernel_unlinker(state.new_kernel);
  ApplyDeltaFile(full_kernel, full_rootfs, noop, signature_test,
                 &state, hash_checks_mandatory, kValidOperationData,
                 &performer);
  VerifyPayload(performer, &state, signature_test);
}

// Calls delta performer's Write method by pretending to pass in bytes from a
// delta file whose metadata size is actual_metadata_size and tests if all
// checks are correctly performed if the install plan contains
// expected_metadata_size and that the result of the parsing are as per
// hash_checks_mandatory flag.
void DoMetadataSizeTest(uint64_t expected_metadata_size,
                        uint64_t actual_metadata_size,
                        bool hash_checks_mandatory) {
  PrefsMock prefs;
  InstallPlan install_plan;
  install_plan.hash_checks_mandatory = hash_checks_mandatory;
  MockSystemState mock_system_state;
  DeltaPerformer performer(&prefs, &mock_system_state, &install_plan);
  EXPECT_EQ(0, performer.Open("/dev/null", 0, 0));
  EXPECT_TRUE(performer.OpenKernel("/dev/null"));

  // Set a valid magic string and version number 1.
  EXPECT_TRUE(performer.Write("CrAU", 4));
  uint64_t version = htobe64(1);
  EXPECT_TRUE(performer.Write(&version, 8));

  install_plan.metadata_size = expected_metadata_size;
  ActionExitCode error_code;
  // When filling in size in manifest, exclude the size of the 20-byte header.
  uint64_t size_in_manifest = htobe64(actual_metadata_size - 20);
  bool result = performer.Write(&size_in_manifest, 8, &error_code);
  if (expected_metadata_size == actual_metadata_size ||
      !hash_checks_mandatory) {
    EXPECT_TRUE(result);
  } else {
    EXPECT_FALSE(result);
    EXPECT_EQ(kActionCodeDownloadInvalidMetadataSize, error_code);
  }

  EXPECT_LT(performer.Close(), 0);
}

// Generates a valid delta file but tests the delta performer by suppling
// different metadata signatures as per omaha_metadata_signature flag and
// sees if the result of the parsing are as per hash_checks_mandatory flag.
void DoMetadataSignatureTest(MetadataSignatureTest metadata_signature_test,
                             SignatureTest signature_test,
                             bool hash_checks_mandatory) {
  DeltaState state;

  // Using kSignatureNone since it doesn't affect the results of our test.
  // If we've to use other signature options, then we'd have to get the
  // metadata size again after adding the signing operation to the manifest.
  GenerateDeltaFile(true, true, false, signature_test, &state);

  ScopedPathUnlinker a_img_unlinker(state.a_img);
  ScopedPathUnlinker b_img_unlinker(state.b_img);
  ScopedPathUnlinker delta_unlinker(state.delta_path);
  ScopedPathUnlinker old_kernel_unlinker(state.old_kernel);
  ScopedPathUnlinker new_kernel_unlinker(state.new_kernel);

  // Loads the payload and parses the manifest.
  vector<char> payload;
  EXPECT_TRUE(utils::ReadFile(state.delta_path, &payload));
  LOG(INFO) << "Payload size: " << payload.size();

  InstallPlan install_plan;
  install_plan.hash_checks_mandatory = hash_checks_mandatory;
  install_plan.metadata_size = state.metadata_size;

  DeltaPerformer::MetadataParseResult expected_result, actual_result;
  ActionExitCode expected_error, actual_error;

  // Fill up the metadata signature in install plan according to the test.
  switch (metadata_signature_test) {
    case kEmptyMetadataSignature:
      install_plan.metadata_signature.clear();
      expected_result = DeltaPerformer::kMetadataParseError;
      expected_error = kActionCodeDownloadMetadataSignatureMissingError;
      break;

    case kInvalidMetadataSignature:
      install_plan.metadata_signature = kBogusMetadataSignature1;
      expected_result = DeltaPerformer::kMetadataParseError;
      expected_error = kActionCodeDownloadMetadataSignatureMismatch;
      break;

    case kValidMetadataSignature:
    default:
      // Set the install plan's metadata size to be the same as the one
      // in the manifest so that we pass the metadata size checks. Only
      // then we can get to manifest signature checks.
      ASSERT_TRUE(PayloadSigner::GetMetadataSignature(
          &payload[0],
          state.metadata_size,
          kUnittestPrivateKeyPath,
          &install_plan.metadata_signature));
      EXPECT_FALSE(install_plan.metadata_signature.empty());
      expected_result = DeltaPerformer::kMetadataParseSuccess;
      expected_error = kActionCodeSuccess;
      break;
  }

  // Ignore the expected result/error if hash checks are not mandatory.
  if (!hash_checks_mandatory) {
    expected_result = DeltaPerformer::kMetadataParseSuccess;
    expected_error = kActionCodeSuccess;
  }

  // Create the delta performer object.
  PrefsMock prefs;
  DeltaPerformer delta_performer(&prefs,
                                 &state.mock_system_state,
                                 &install_plan);

  // Use the public key corresponding to the private key used above to
  // sign the metadata.
  EXPECT_TRUE(utils::FileExists(kUnittestPublicKeyPath));
  delta_performer.set_public_key_path(kUnittestPublicKeyPath);

  // Parse the delta payload we created.
  DeltaArchiveManifest manifest;
  uint64_t parsed_metadata_size;

  // Init actual_error with an invalid value so that we make sure
  // ParsePayloadMetadata properly populates it in all cases.
  actual_error = kActionCodeUmaReportedMax;
  actual_result = delta_performer.ParsePayloadMetadata(payload, &manifest,
      &parsed_metadata_size, &actual_error);

  EXPECT_EQ(expected_result, actual_result);
  EXPECT_EQ(expected_error, actual_error);

  // Check that the parsed metadata size is what's expected. This test
  // implicitly confirms that the metadata signature is valid, if required.
  EXPECT_EQ(state.metadata_size, parsed_metadata_size);
}

void DoOperationHashMismatchTest(OperationHashTest op_hash_test,
                                 bool hash_checks_mandatory) {
  DeltaState state;
  GenerateDeltaFile(true, true, false, kSignatureGenerated, &state);
  ScopedPathUnlinker a_img_unlinker(state.a_img);
  ScopedPathUnlinker b_img_unlinker(state.b_img);
  ScopedPathUnlinker delta_unlinker(state.delta_path);
  ScopedPathUnlinker old_kernel_unlinker(state.old_kernel);
  ScopedPathUnlinker new_kernel_unlinker(state.new_kernel);
  DeltaPerformer *performer;
  ApplyDeltaFile(true, true, false, kSignatureGenerated,
                 &state, hash_checks_mandatory, op_hash_test, &performer);
}

class DeltaPerformerTest : public ::testing::Test { };

TEST(DeltaPerformerTest, ExtentsToByteStringTest) {
  uint64_t test[] = {1, 1, 4, 2, kSparseHole, 1, 0, 1};
  COMPILE_ASSERT(arraysize(test) % 2 == 0, array_size_uneven);
  const uint64_t block_size = 4096;
  const uint64_t file_length = 5 * block_size - 13;

  google::protobuf::RepeatedPtrField<Extent> extents;
  for (size_t i = 0; i < arraysize(test); i += 2) {
    Extent* extent = extents.Add();
    extent->set_start_block(test[i]);
    extent->set_num_blocks(test[i + 1]);
  }

  string expected_output = "4096:4096,16384:8192,-1:4096,0:4083";
  string actual_output;
  EXPECT_TRUE(DeltaPerformer::ExtentsToBsdiffPositionsString(extents,
                                                             block_size,
                                                             file_length,
                                                             &actual_output));
  EXPECT_EQ(expected_output, actual_output);
}

TEST(DeltaPerformerTest, RunAsRootSmallImageTest) {
  bool hash_checks_mandatory = false;
  DoSmallImageTest(false, false, false, kSignatureGenerator,
                   hash_checks_mandatory);
}

TEST(DeltaPerformerTest, RunAsRootFullKernelSmallImageTest) {
  bool hash_checks_mandatory = false;
  DoSmallImageTest(true, false, false, kSignatureGenerator,
                   hash_checks_mandatory);
}

TEST(DeltaPerformerTest, RunAsRootFullSmallImageTest) {
  bool hash_checks_mandatory = true;
  DoSmallImageTest(true, true, false, kSignatureGenerator,
                   hash_checks_mandatory);
}

TEST(DeltaPerformerTest, RunAsRootNoopSmallImageTest) {
  bool hash_checks_mandatory = false;
  DoSmallImageTest(false, false, true, kSignatureGenerator,
                   hash_checks_mandatory);
}

TEST(DeltaPerformerTest, RunAsRootSmallImageSignNoneTest) {
  bool hash_checks_mandatory = false;
  DoSmallImageTest(false, false, false, kSignatureNone,
                   hash_checks_mandatory);
}

TEST(DeltaPerformerTest, RunAsRootSmallImageSignGeneratedTest) {
  bool hash_checks_mandatory = true;
  DoSmallImageTest(false, false, false, kSignatureGenerated,
                   hash_checks_mandatory);
}

TEST(DeltaPerformerTest, RunAsRootSmallImageSignGeneratedShellTest) {
  bool hash_checks_mandatory = false;
  DoSmallImageTest(false, false, false, kSignatureGeneratedShell,
                   hash_checks_mandatory);
}

TEST(DeltaPerformerTest, RunAsRootSmallImageSignGeneratedShellBadKeyTest) {
  bool hash_checks_mandatory = false;
  DoSmallImageTest(false, false, false, kSignatureGeneratedShellBadKey,
                   hash_checks_mandatory);
}

TEST(DeltaPerformerTest, RunAsRootSmallImageSignGeneratedShellRotateCl1Test) {
  bool hash_checks_mandatory = false;
  DoSmallImageTest(false, false, false, kSignatureGeneratedShellRotateCl1,
                   hash_checks_mandatory);
}

TEST(DeltaPerformerTest, RunAsRootSmallImageSignGeneratedShellRotateCl2Test) {
  bool hash_checks_mandatory = false;
  DoSmallImageTest(false, false, false, kSignatureGeneratedShellRotateCl2,
                   hash_checks_mandatory);
}

TEST(DeltaPerformerTest, BadDeltaMagicTest) {
  PrefsMock prefs;
  InstallPlan install_plan;
  MockSystemState mock_system_state;
  DeltaPerformer performer(&prefs, &mock_system_state, &install_plan);
  EXPECT_EQ(0, performer.Open("/dev/null", 0, 0));
  EXPECT_TRUE(performer.OpenKernel("/dev/null"));
  EXPECT_TRUE(performer.Write("junk", 4));
  EXPECT_TRUE(performer.Write("morejunk", 8));
  EXPECT_FALSE(performer.Write("morejunk", 8));
  EXPECT_LT(performer.Close(), 0);
}

TEST(DeltaPerformerTest, IsIdempotentOperationTest) {
  DeltaArchiveManifest_InstallOperation op;
  EXPECT_TRUE(DeltaPerformer::IsIdempotentOperation(op));
  *(op.add_dst_extents()) = ExtentForRange(0, 5);
  EXPECT_TRUE(DeltaPerformer::IsIdempotentOperation(op));
  *(op.add_src_extents()) = ExtentForRange(4, 1);
  EXPECT_FALSE(DeltaPerformer::IsIdempotentOperation(op));
  op.clear_src_extents();
  *(op.add_src_extents()) = ExtentForRange(5, 3);
  EXPECT_TRUE(DeltaPerformer::IsIdempotentOperation(op));
  *(op.add_dst_extents()) = ExtentForRange(20, 6);
  EXPECT_TRUE(DeltaPerformer::IsIdempotentOperation(op));
  *(op.add_src_extents()) = ExtentForRange(19, 2);
  EXPECT_FALSE(DeltaPerformer::IsIdempotentOperation(op));
}

TEST(DeltaPerformerTest, WriteUpdatesPayloadState) {
  PrefsMock prefs;
  InstallPlan install_plan;
  MockSystemState mock_system_state;
  DeltaPerformer performer(&prefs, &mock_system_state, &install_plan);
  EXPECT_EQ(0, performer.Open("/dev/null", 0, 0));
  EXPECT_TRUE(performer.OpenKernel("/dev/null"));

  EXPECT_CALL(*(mock_system_state.mock_payload_state()),
              DownloadProgress(4)).Times(1);
  EXPECT_CALL(*(mock_system_state.mock_payload_state()),
              DownloadProgress(8)).Times(2);

  EXPECT_TRUE(performer.Write("junk", 4));
  EXPECT_TRUE(performer.Write("morejunk", 8));
  EXPECT_FALSE(performer.Write("morejunk", 8));
  EXPECT_LT(performer.Close(), 0);
}

TEST(DeltaPerformerTest, MissingMandatoryMetadataSizeTest) {
  DoMetadataSizeTest(0, 75456, true);
}

TEST(DeltaPerformerTest, MissingNonMandatoryMetadataSizeTest) {
  DoMetadataSizeTest(0, 123456, false);
}

TEST(DeltaPerformerTest, InvalidMandatoryMetadataSizeTest) {
  DoMetadataSizeTest(13000, 140000, true);
}

TEST(DeltaPerformerTest, InvalidNonMandatoryMetadataSizeTest) {
  DoMetadataSizeTest(40000, 50000, false);
}

TEST(DeltaPerformerTest, ValidMandatoryMetadataSizeTest) {
  DoMetadataSizeTest(85376, 85376, true);
}

TEST(DeltaPerformerTest, RunAsRootMandatoryEmptyMetadataSignatureTest) {
  DoMetadataSignatureTest(kEmptyMetadataSignature, kSignatureGenerated, true);
}

TEST(DeltaPerformerTest, RunAsRootNonMandatoryEmptyMetadataSignatureTest) {
  DoMetadataSignatureTest(kEmptyMetadataSignature, kSignatureGenerated, false);
}

TEST(DeltaPerformerTest, RunAsRootMandatoryInvalidMetadataSignatureTest) {
  DoMetadataSignatureTest(kInvalidMetadataSignature, kSignatureGenerated, true);
}

TEST(DeltaPerformerTest, RunAsRootNonMandatoryInvalidMetadataSignatureTest) {
  DoMetadataSignatureTest(kInvalidMetadataSignature, kSignatureGenerated,
                          false);
}

TEST(DeltaPerformerTest, RunAsRootMandatoryValidMetadataSignature1Test) {
  DoMetadataSignatureTest(kValidMetadataSignature, kSignatureNone, true);
}

TEST(DeltaPerformerTest, RunAsRootMandatoryValidMetadataSignature2Test) {
  DoMetadataSignatureTest(kValidMetadataSignature, kSignatureGenerated, true);
}

TEST(DeltaPerformerTest, RunAsRootNonMandatoryValidMetadataSignatureTest) {
  DoMetadataSignatureTest(kValidMetadataSignature, kSignatureGenerated, false);
}

TEST(DeltaPerformerTest, RunAsRootMandatoryOperationHashMismatchTest) {
  DoOperationHashMismatchTest(kInvalidOperationData, true);
}

}  // namespace chromeos_update_engine
