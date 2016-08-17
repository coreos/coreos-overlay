// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mount.h>
#include <sys/types.h>
#include <inttypes.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include <google/protobuf/repeated_field.h>
#include <gtest/gtest.h>

#include "files/file_util.h"
#include "files/scoped_file.h"
#include "strings/string_printf.h"
#include "update_engine/delta_diff_generator.h"
#include "update_engine/extent_ranges.h"
#include "update_engine/full_update_generator.h"
#include "update_engine/graph_types.h"
#include "update_engine/payload_processor.h"
#include "update_engine/payload_signer.h"
#include "update_engine/prefs_mock.h"
#include "update_engine/test_utils.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

namespace chromeos_update_engine {

using std::min;
using std::string;
using std::vector;
using strings::StringPrintf;
using testing::_;
using testing::Return;

extern const char* kUnittestPrivateKeyPath;
extern const char* kUnittestPublicKeyPath;
extern const char* kUnittestPrivateKey2Path;
extern const char* kUnittestPublicKey2Path;

static const size_t kBlockSize = 4096;

static const int kDefaultKernelSize = 500; // Something small for a test
static const char* kNewDataString = "This is new data.";

namespace {
enum DeltaTest {
  kDeltaUpdate,  // Update by applying a diff.
  kFullUpdate,  // Update by writing full image.
  kNoopUpdate,  // Update with an empty diff.
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

enum OperationHashTest {
  kInvalidOperationData,
  kValidOperationData,
};

struct DeltaState {
  DeltaTest delta_test;
  SignatureTest signature_test;
  InstallPlan install_plan;

  string a_img;
  string b_img;
  off_t image_size;

  string delta_path;
  uint64_t metadata_size;

  string a_kernel;
  vector<char> a_kernel_data;

  string b_kernel;
  vector<char> b_kernel_data;

  // The in-memory copy of delta file.
  vector<char> delta;
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
  files::ScopedFD fd_closer(fd);
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
      kSignatureMessageCurrentVersion));
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

static void GenerateDeltaFile(DeltaState *state) {
  EXPECT_TRUE(utils::MakeTempFile("/tmp/a_img.XXXXXX", &state->a_img, NULL));
  EXPECT_TRUE(utils::MakeTempFile("/tmp/b_img.XXXXXX", &state->b_img, NULL));
  CreateExtImageAtPath(state->a_img, NULL);

  state->image_size = utils::FileSize(state->a_img);

  // Extend the "partitions" holding the file system a bit.
  state->image_size += 1024 * 1024;
  EXPECT_EQ(0, truncate(state->a_img.c_str(), state->image_size));

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

  if (state->delta_test == kNoopUpdate) {
    EXPECT_TRUE(files::CopyFile(files::FilePath(state->a_img),
                                files::FilePath(state->b_img)));
  } else {
    CreateExtImageAtPath(state->b_img, NULL);
    EXPECT_EQ(0, truncate(state->b_img.c_str(), state->image_size));

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

  EXPECT_TRUE(utils::MakeTempFile("/tmp/a_kernel.XXXXXX",
                                  &state->a_kernel,
                                  NULL));

  EXPECT_TRUE(utils::MakeTempFile("/tmp/b_kernel.XXXXXX",
                                  &state->b_kernel,
                                  NULL));

  state->b_kernel_data.resize(kDefaultKernelSize);
  FillWithData(&state->b_kernel_data);
  // A known bit of data to test for later.
  strcpy(&state->b_kernel_data[0], kNewDataString);

  if (state->delta_test == kNoopUpdate)
    state->a_kernel_data = state->b_kernel_data;
  else
    FillWithData(&state->a_kernel_data);

  // Write kernels to disk
  EXPECT_TRUE(utils::WriteFile(state->a_kernel.c_str(),
                               &state->a_kernel_data[0],
                               state->a_kernel_data.size()));
  EXPECT_TRUE(utils::WriteFile(state->b_kernel.c_str(),
                               &state->b_kernel_data[0],
                               state->b_kernel_data.size()));

  EXPECT_TRUE(utils::MakeTempFile("/tmp/delta.XXXXXX",
                                  &state->delta_path,
                                  NULL));
  LOG(INFO) << "delta path: " << state->delta_path;
  {
    string a_mnt, b_mnt;
    ScopedLoopMounter a_mounter(state->a_img, &a_mnt, MS_RDONLY);
    ScopedLoopMounter b_mounter(state->b_img, &b_mnt, MS_RDONLY);
    const bool full_update = state->delta_test == kFullUpdate;
    const string private_key =
        state->signature_test == kSignatureGenerator
        ? kUnittestPrivateKeyPath : "";
    EXPECT_TRUE(
        DeltaDiffGenerator::GenerateDeltaUpdateFile(
            full_update ? "" : a_mnt,
            full_update ? "" : state->a_img,
            b_mnt,
            state->b_img,
            full_update ? "" : state->a_kernel,
            state->b_kernel,
            state->delta_path,
            private_key,
            &state->metadata_size));
  }

  switch (state->signature_test) {
    case kSignatureGenerated:
      // Generate the signed payload and update the metadata size in state to
      // reflect the new size after adding the signature operation to the
      // manifest.
      SignGeneratedPayload(state->delta_path, &state->metadata_size);
      break;
    case kSignatureGeneratedShell:
    case kSignatureGeneratedShellBadKey:
    case kSignatureGeneratedShellRotateCl1:
    case kSignatureGeneratedShellRotateCl2:
      SignGeneratedShellPayload(state->signature_test, state->delta_path);
      break;
    case kSignatureNone:
    case kSignatureGenerator:
      break;
  }
}

static const InstallProcedure* GetProcedure(
    const DeltaArchiveManifest& manifest,
    InstallProcedure_Type type) {
  for (const InstallProcedure& proc : manifest.procedures()) {
    if (proc.type() == type) {
      return &proc;
    }
  }
  return nullptr;
}

static void ApplyDeltaFile(DeltaState* state,
                           OperationHashTest op_hash_test,
                           PayloadProcessor** performer) {
  // Check the metadata.
  {
    DeltaArchiveManifest manifest;
    EXPECT_TRUE(PayloadSigner::LoadPayload(state->delta_path,
                                           &state->delta,
                                           &manifest,
                                           &state->metadata_size));
    LOG(INFO) << "Metadata size: " << state->metadata_size;

    // manifest.noop_operations contains dummy operations for everything
    // in the new manifest.procedures list and one for the signature data.
    int noop_operations_size = 0;

    if (state->signature_test == kSignatureNone) {
      EXPECT_FALSE(manifest.has_signatures_offset());
      EXPECT_FALSE(manifest.has_signatures_size());
    } else {
      noop_operations_size++; // one operation for signature data.
      EXPECT_TRUE(manifest.has_signatures_offset());
      EXPECT_TRUE(manifest.has_signatures_size());
      Signatures sigs_message;
      EXPECT_TRUE(sigs_message.ParseFromArray(
          &state->delta[state->metadata_size + manifest.signatures_offset()],
          manifest.signatures_size()));
      if (state->signature_test == kSignatureGeneratedShellRotateCl1 ||
          state->signature_test == kSignatureGeneratedShellRotateCl2)
        EXPECT_EQ(2, sigs_message.signatures_size());
      else
        EXPECT_EQ(1, sigs_message.signatures_size());
      const Signatures_Signature& signature = sigs_message.signatures(0);
      EXPECT_EQ(kSignatureMessageCurrentVersion, signature.version());

      uint64_t expected_sig_data_length = 0;
      vector<string> key_paths (1, kUnittestPrivateKeyPath);
      if (state->signature_test == kSignatureGeneratedShellRotateCl1 ||
          state->signature_test == kSignatureGeneratedShellRotateCl2) {
        key_paths.push_back(kUnittestPrivateKey2Path);
      }
      EXPECT_TRUE(PayloadSigner::SignatureBlobLength(
          key_paths,
          &expected_sig_data_length));
      EXPECT_EQ(expected_sig_data_length, manifest.signatures_size());
      EXPECT_FALSE(signature.data().empty());
    }

    const InstallProcedure* kernel_proc =
        GetProcedure(manifest, InstallProcedure_Type_KERNEL);
    ASSERT_NE(nullptr, kernel_proc);
    noop_operations_size += kernel_proc->operations_size();

    switch (state->delta_test) {
      case kNoopUpdate:
        // Computing the filesystem delta does not compare unallocated and
        // out of bounds blocks, those simply get packed into a REPLACE_BZ
        // operation to directly write out the data (mostly zero).
        EXPECT_EQ(1, manifest.partition_operations_size());
        // Kernels on the other hand do not have any extra data like that.
        EXPECT_EQ(0, kernel_proc->operations_size());
        // fallthrough
      case kDeltaUpdate:
        EXPECT_EQ(state->image_size, manifest.old_partition_info().size());
        EXPECT_EQ(state->a_kernel_data.size(), kernel_proc->old_info().size());
        EXPECT_FALSE(manifest.old_partition_info().hash().empty());
        EXPECT_FALSE(kernel_proc->old_info().hash().empty());
        break;
      case kFullUpdate:
        EXPECT_FALSE(manifest.has_old_partition_info());
        EXPECT_FALSE(kernel_proc->has_old_info());
        break;
    }

    EXPECT_EQ(state->image_size, manifest.new_partition_info().size());
    EXPECT_EQ(state->b_kernel_data.size(), kernel_proc->new_info().size());
    EXPECT_FALSE(manifest.new_partition_info().hash().empty());
    EXPECT_FALSE(kernel_proc->new_info().hash().empty());

    EXPECT_EQ(noop_operations_size, manifest.noop_operations_size());
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
  if (op_hash_test == kValidOperationData &&
      state->signature_test != kSignatureNone) {
    EXPECT_CALL(prefs, SetString(kPrefsUpdateStateSignedSHA256Context, _))
        .WillOnce(Return(true));
    EXPECT_CALL(prefs, SetString(kPrefsUpdateStateSignatureBlob, _))
        .WillOnce(Return(true));
  }

  // Update the A image in place.
  state->install_plan.partition_path = state->a_img;
  // TODO: Kernel support in PayloadProcessor
  state->install_plan.payload_size = state->delta.size();
  state->install_plan.payload_hash =
      OmahaHashCalculator::OmahaHashOfData(state->delta);

  *performer = new PayloadProcessor(&prefs, &state->install_plan);
  EXPECT_TRUE(utils::FileExists(kUnittestPublicKeyPath));
  (*performer)->set_public_key_path(kUnittestPublicKeyPath);

  EXPECT_EQ(state->image_size,
            OmahaHashCalculator::RawHashOfFile(
              state->a_img, state->image_size,
              &state->install_plan.old_partition_hash));

  EXPECT_EQ(0, (*performer)->Open());

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

void VerifyPayloadResult(PayloadProcessor* performer,
                         DeltaState* state,
                         ActionExitCode expected_result) {
  if (!performer) {
    EXPECT_TRUE(!"Skipping payload verification since performer is NULL.");
    return;
  }

  LOG(INFO) << "Verifying payload for expected result "
            << expected_result;
  EXPECT_EQ(expected_result, performer->VerifyPayload());
  LOG(INFO) << "Verified payload.";

  if (expected_result != kActionCodeSuccess) {
    // no need to verify new partition if VerifyPayload failed.
    return;
  }

  CompareFilesByBlock(state->a_img, state->b_img);

  EXPECT_EQ(state->image_size, state->install_plan.new_partition_size);
  vector<char> expected_partition_hash;
  EXPECT_EQ(state->image_size,
            OmahaHashCalculator::RawHashOfFile(state->b_img,
                                               state->image_size,
                                               &expected_partition_hash));
  EXPECT_TRUE(expected_partition_hash ==
              state->install_plan.new_partition_hash);
}

void VerifyPayload(PayloadProcessor* performer, DeltaState* state) {
  ActionExitCode expected_result = kActionCodeSuccess;
  switch (state->signature_test) {
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

void DoSmallImageTest(DeltaState *state) {
  PayloadProcessor *performer;
  GenerateDeltaFile(state);
  ScopedPathUnlinker a_img_unlinker(state->a_img);
  ScopedPathUnlinker b_img_unlinker(state->b_img);
  ScopedPathUnlinker delta_unlinker(state->delta_path);
  ApplyDeltaFile(state, kValidOperationData, &performer);
  VerifyPayload(performer, state);
}

void DoOperationHashMismatchTest(OperationHashTest op_hash_test) {
  DeltaState state;
  state.delta_test = kFullUpdate;
  state.signature_test = kSignatureGenerated;

  GenerateDeltaFile(&state);
  ScopedPathUnlinker a_img_unlinker(state.a_img);
  ScopedPathUnlinker b_img_unlinker(state.b_img);
  ScopedPathUnlinker delta_unlinker(state.delta_path);
  PayloadProcessor *performer;
  ApplyDeltaFile(&state, op_hash_test, &performer);
}

class PayloadProcessorTest : public ::testing::Test { };

TEST(PayloadProcessorTest, RunAsRootSmallImageTest) {
  DeltaState state;
  state.delta_test = kDeltaUpdate;
  state.signature_test = kSignatureGenerator;

  DoSmallImageTest(&state);
}

TEST(PayloadProcessorTest, RunAsRootFullSmallImageTest) {
  DeltaState state;
  state.delta_test = kFullUpdate;
  state.signature_test = kSignatureGenerator;

  DoSmallImageTest(&state);
}

TEST(PayloadProcessorTest, RunAsRootNoopSmallImageTest) {
  DeltaState state;
  state.delta_test = kNoopUpdate;
  state.signature_test = kSignatureGenerator;

  DoSmallImageTest(&state);
}

TEST(PayloadProcessorTest, RunAsRootSmallImageSignNoneTest) {
  DeltaState state;
  state.delta_test = kDeltaUpdate;
  state.signature_test = kSignatureNone;

  DoSmallImageTest(&state);
}

TEST(PayloadProcessorTest, RunAsRootSmallImageSignGeneratedTest) {
  DeltaState state;
  state.delta_test = kDeltaUpdate;
  state.signature_test = kSignatureGenerated;

  DoSmallImageTest(&state);
}

TEST(PayloadProcessorTest, BadDeltaMagicTest) {
  PrefsMock prefs;
  InstallPlan install_plan;
  install_plan.partition_path = "/dev/null";
  PayloadProcessor performer(&prefs, &install_plan);
  EXPECT_EQ(0, performer.Open());
  EXPECT_TRUE(performer.Write("junk", 4));
  EXPECT_TRUE(performer.Write("morejunk", 8));
  EXPECT_FALSE(performer.Write("morejunk", 8));
  EXPECT_LT(performer.Close(), 0);
}

TEST(PayloadProcessorTest, RunAsRootOperationHashMismatchTest) {
  DoOperationHashMismatchTest(kInvalidOperationData);
}

}  // namespace chromeos_update_engine
