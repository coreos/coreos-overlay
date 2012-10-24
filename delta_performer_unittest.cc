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

namespace {
const size_t kBlockSize = 4096;
}  // namespace {}


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

void CompareFilesByBlock(const string& a_file, const string& b_file) {
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

namespace {
bool WriteSparseFile(const string& path, off_t size) {
  int fd = open(path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
  TEST_AND_RETURN_FALSE_ERRNO(fd >= 0);
  ScopedFdCloser fd_closer(&fd);
  off_t rc = lseek(fd, size + 1, SEEK_SET);
  TEST_AND_RETURN_FALSE_ERRNO(rc != static_cast<off_t>(-1));
  int return_code = ftruncate(fd, size);
  TEST_AND_RETURN_FALSE_ERRNO(return_code == 0);
  return true;
}
}  // namespace {}

namespace {
enum SignatureTest {
  kSignatureNone,  // No payload signing.
  kSignatureGenerator,  // Sign the payload at generation time.
  kSignatureGenerated,  // Sign the payload after it's generated.
  kSignatureGeneratedShell,  // Sign the generated payload through shell cmds.
  kSignatureGeneratedShellBadKey,  // Sign with a bad key through shell cmds.
  kSignatureGeneratedShellRotateCl1,  // Rotate key, test client v1
  kSignatureGeneratedShellRotateCl2,  // Rotate key, test client v2
};

size_t GetSignatureSize(const string& private_key_path) {
  const vector<char> data(1, 'x');
  vector<char> hash;
  EXPECT_TRUE(OmahaHashCalculator::RawHashOfData(data, &hash));
  vector<char> signature;
  EXPECT_TRUE(PayloadSigner::SignHash(hash,
                                      private_key_path,
                                      &signature));
  return signature.size();
}

void SignGeneratedPayload(const string& payload_path) {
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
      payload_path));
  EXPECT_TRUE(PayloadSigner::VerifySignedPayload(
      payload_path,
      kUnittestPublicKeyPath,
      kSignatureMessageOriginalVersion));
}

void SignGeneratedShellPayload(SignatureTest signature_test,
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

void DoSmallImageTest(bool full_kernel, bool full_rootfs, bool noop,
                      SignatureTest signature_test) {
  string a_img, b_img;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/a_img.XXXXXX", &a_img, NULL));
  ScopedPathUnlinker a_img_unlinker(a_img);
  EXPECT_TRUE(utils::MakeTempFile("/tmp/b_img.XXXXXX", &b_img, NULL));
  ScopedPathUnlinker b_img_unlinker(b_img);

  CreateExtImageAtPath(a_img, NULL);

  int image_size = static_cast<int>(utils::FileSize(a_img));

  // Extend the "partitions" holding the file system a bit.
  EXPECT_EQ(0, System(base::StringPrintf(
      "dd if=/dev/zero of=%s seek=%d bs=1 count=1",
      a_img.c_str(),
      image_size + 1024 * 1024 - 1)));
  EXPECT_EQ(image_size + 1024 * 1024, utils::FileSize(a_img));

  // Make some changes to the A image.
  {
    string a_mnt;
    ScopedLoopMounter b_mounter(a_img, &a_mnt, 0);

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
    EXPECT_TRUE(file_util::CopyFile(FilePath(a_img), FilePath(b_img)));
  } else {
    CreateExtImageAtPath(b_img, NULL);
    EXPECT_EQ(0, System(base::StringPrintf(
        "dd if=/dev/zero of=%s seek=%d bs=1 count=1",
        b_img.c_str(),
        image_size + 1024 * 1024 - 1)));
    EXPECT_EQ(image_size + 1024 * 1024, utils::FileSize(b_img));

    // Make some changes to the B image.
    string b_mnt;
    ScopedLoopMounter b_mounter(b_img, &b_mnt, 0);

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
  EXPECT_TRUE(utils::MakeTempFile("/tmp/old_kernel.XXXXXX", &old_kernel, NULL));
  ScopedPathUnlinker old_kernel_unlinker(old_kernel);

  string new_kernel;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/new_kernel.XXXXXX", &new_kernel, NULL));
  ScopedPathUnlinker new_kernel_unlinker(new_kernel);

  vector<char> old_kernel_data(4096);  // Something small for a test
  vector<char> new_kernel_data(old_kernel_data.size());
  FillWithData(&old_kernel_data);
  FillWithData(&new_kernel_data);

  // change the new kernel data
  const char* new_data_string = "This is new data.";
  strcpy(&new_kernel_data[0], new_data_string);

  if (noop) {
    old_kernel_data = new_kernel_data;
  }

  // Write kernels to disk
  EXPECT_TRUE(utils::WriteFile(
      old_kernel.c_str(), &old_kernel_data[0], old_kernel_data.size()));
  EXPECT_TRUE(utils::WriteFile(
      new_kernel.c_str(), &new_kernel_data[0], new_kernel_data.size()));

  string delta_path;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/delta.XXXXXX", &delta_path, NULL));
  LOG(INFO) << "delta path: " << delta_path;
  ScopedPathUnlinker delta_path_unlinker(delta_path);
  {
    string a_mnt, b_mnt;
    ScopedLoopMounter a_mounter(a_img, &a_mnt, MS_RDONLY);
    ScopedLoopMounter b_mounter(b_img, &b_mnt, MS_RDONLY);
    const string private_key =
        signature_test == kSignatureGenerator ? kUnittestPrivateKeyPath : "";
    EXPECT_TRUE(
        DeltaDiffGenerator::GenerateDeltaUpdateFile(
            full_rootfs ? "" : a_mnt,
            full_rootfs ? "" : a_img,
            b_mnt,
            b_img,
            full_kernel ? "" : old_kernel,
            new_kernel,
            delta_path,
            private_key));
  }

  if (signature_test == kSignatureGenerated) {
    SignGeneratedPayload(delta_path);
  } else if (signature_test == kSignatureGeneratedShell ||
             signature_test == kSignatureGeneratedShellBadKey ||
             signature_test == kSignatureGeneratedShellRotateCl1 ||
             signature_test == kSignatureGeneratedShellRotateCl2) {
    SignGeneratedShellPayload(signature_test, delta_path);
  }

  // Read delta into memory.
  vector<char> delta;
  uint64_t metadata_size;

  // Check the metadata.
  {
    DeltaArchiveManifest manifest;
    EXPECT_TRUE(PayloadSigner::LoadPayload(delta_path, &delta, &manifest,
                                           &metadata_size));
    LOG(INFO) << "Metadata size: " << metadata_size;

    if (signature_test == kSignatureNone) {
      EXPECT_FALSE(manifest.has_signatures_offset());
      EXPECT_FALSE(manifest.has_signatures_size());
    } else {
      EXPECT_TRUE(manifest.has_signatures_offset());
      EXPECT_TRUE(manifest.has_signatures_size());
      Signatures sigs_message;
      EXPECT_TRUE(sigs_message.ParseFromArray(
          &delta[metadata_size + manifest.signatures_offset()],
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
      EXPECT_EQ(old_kernel_data.size(), manifest.old_kernel_info().size());
      EXPECT_FALSE(manifest.old_kernel_info().hash().empty());
    }

    if (full_rootfs) {
      EXPECT_FALSE(manifest.has_old_rootfs_info());
    } else {
      EXPECT_EQ(image_size, manifest.old_rootfs_info().size());
      EXPECT_FALSE(manifest.old_rootfs_info().hash().empty());
    }

    EXPECT_EQ(new_kernel_data.size(), manifest.new_kernel_info().size());
    EXPECT_EQ(image_size, manifest.new_rootfs_info().size());

    EXPECT_FALSE(manifest.new_kernel_info().hash().empty());
    EXPECT_FALSE(manifest.new_rootfs_info().hash().empty());
  }

  PrefsMock prefs;
  EXPECT_CALL(prefs, SetInt64(kPrefsManifestMetadataSize,
                              metadata_size)).WillOnce(Return(true));
  EXPECT_CALL(prefs, SetInt64(kPrefsUpdateStateNextOperation, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(prefs, GetInt64(kPrefsUpdateStateNextOperation, _))
      .WillOnce(Return(false));
  EXPECT_CALL(prefs, SetInt64(kPrefsUpdateStateNextDataOffset, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(prefs, SetString(kPrefsUpdateStateSHA256Context, _))
      .WillRepeatedly(Return(true));
  if (signature_test != kSignatureNone) {
    EXPECT_CALL(prefs, SetString(kPrefsUpdateStateSignedSHA256Context, _))
        .WillOnce(Return(true));
    EXPECT_CALL(prefs, SetString(kPrefsUpdateStateSignatureBlob, _))
        .WillOnce(Return(true));
  }

  // Update the A image in place.
  InstallPlan install_plan;
  MockSystemState mock_system_state;
  DeltaPerformer performer(&prefs, &mock_system_state, &install_plan);
  EXPECT_TRUE(utils::FileExists(kUnittestPublicKeyPath));
  performer.set_public_key_path(kUnittestPublicKeyPath);

  EXPECT_EQ(image_size,
            OmahaHashCalculator::RawHashOfFile(a_img,
                                               image_size,
                                               &install_plan.rootfs_hash));
  EXPECT_TRUE(OmahaHashCalculator::RawHashOfData(old_kernel_data,
                                                 &install_plan.kernel_hash));

  EXPECT_EQ(0, performer.Open(a_img.c_str(), 0, 0));
  EXPECT_TRUE(performer.OpenKernel(old_kernel.c_str()));

  // Write at some number of bytes per operation. Arbitrarily chose 5.
  const size_t kBytesPerWrite = 5;
  for (size_t i = 0; i < delta.size(); i += kBytesPerWrite) {
    size_t count = min(delta.size() - i, kBytesPerWrite);
    EXPECT_TRUE(performer.Write(&delta[i], count));
  }

  // Wrapper around close. Returns 0 on success or -errno on error.
  EXPECT_EQ(0, performer.Close());

  CompareFilesByBlock(old_kernel, new_kernel);
  CompareFilesByBlock(a_img, b_img);

  vector<char> updated_kernel_partition;
  EXPECT_TRUE(utils::ReadFile(old_kernel, &updated_kernel_partition));
  EXPECT_EQ(0, strncmp(&updated_kernel_partition[0], new_data_string,
                       strlen(new_data_string)));

  ActionExitCode expect_verify_result = kActionCodeSuccess;
  switch (signature_test) {
    case kSignatureNone:
      expect_verify_result = kActionCodeSignedDeltaPayloadExpectedError;
      break;
    case kSignatureGeneratedShellBadKey:
      expect_verify_result = kActionCodeDownloadPayloadPubKeyVerificationError;
      break;
    default: break;  // appease gcc
  }
  EXPECT_EQ(expect_verify_result, performer.VerifyPayload(
      OmahaHashCalculator::OmahaHashOfData(delta),
      delta.size()));

  performer.set_public_key_path("/public/key/does/not/exists");
  EXPECT_EQ(kActionCodeSuccess, performer.VerifyPayload(
      OmahaHashCalculator::OmahaHashOfData(delta),
      delta.size()));

  uint64_t new_kernel_size;
  vector<char> new_kernel_hash;
  uint64_t new_rootfs_size;
  vector<char> new_rootfs_hash;
  EXPECT_TRUE(performer.GetNewPartitionInfo(&new_kernel_size,
                                            &new_kernel_hash,
                                            &new_rootfs_size,
                                            &new_rootfs_hash));
  EXPECT_EQ(4096, new_kernel_size);
  vector<char> expected_new_kernel_hash;
  EXPECT_TRUE(OmahaHashCalculator::RawHashOfData(new_kernel_data,
                                                 &expected_new_kernel_hash));
  EXPECT_TRUE(expected_new_kernel_hash == new_kernel_hash);
  EXPECT_EQ(image_size, new_rootfs_size);
  vector<char> expected_new_rootfs_hash;
  EXPECT_EQ(image_size,
            OmahaHashCalculator::RawHashOfFile(b_img,
                                               image_size,
                                               &expected_new_rootfs_hash));
  EXPECT_TRUE(expected_new_rootfs_hash == new_rootfs_hash);
}

// TODO(jaysri): Refactor the previous unit test so we can reuse a lot of
// code between these two methods.
void DoManifestTest() {
  bool full_kernel = false;
  bool full_rootfs = false;
  string a_img, b_img;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/a_img.XXXXXX", &a_img, NULL));
  ScopedPathUnlinker a_img_unlinker(a_img);
  EXPECT_TRUE(utils::MakeTempFile("/tmp/b_img.XXXXXX", &b_img, NULL));
  ScopedPathUnlinker b_img_unlinker(b_img);

  CreateExtImageAtPath(a_img, NULL);

  int image_size = static_cast<int>(utils::FileSize(a_img));

  // Extend the "partitions" holding the file system a bit.
  EXPECT_EQ(0, System(base::StringPrintf(
      "dd if=/dev/zero of=%s seek=%d bs=1 count=1",
      a_img.c_str(),
      image_size + 1024 * 1024 - 1)));
  EXPECT_EQ(image_size + 1024 * 1024, utils::FileSize(a_img));

  // Make some changes to the A image.
  {
    string a_mnt;
    ScopedLoopMounter b_mounter(a_img, &a_mnt, 0);

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

  {
    CreateExtImageAtPath(b_img, NULL);
    EXPECT_EQ(0, System(base::StringPrintf(
        "dd if=/dev/zero of=%s seek=%d bs=1 count=1",
        b_img.c_str(),
        image_size + 1024 * 1024 - 1)));
    EXPECT_EQ(image_size + 1024 * 1024, utils::FileSize(b_img));

    // Make some changes to the B image.
    string b_mnt;
    ScopedLoopMounter b_mounter(b_img, &b_mnt, 0);

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
  EXPECT_TRUE(utils::MakeTempFile("/tmp/old_kernel.XXXXXX", &old_kernel, NULL));
  ScopedPathUnlinker old_kernel_unlinker(old_kernel);

  string new_kernel;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/new_kernel.XXXXXX", &new_kernel, NULL));
  ScopedPathUnlinker new_kernel_unlinker(new_kernel);

  vector<char> old_kernel_data(4096);  // Something small for a test
  vector<char> new_kernel_data(old_kernel_data.size());
  FillWithData(&old_kernel_data);
  FillWithData(&new_kernel_data);

  // change the new kernel data
  const char* new_data_string = "This is new data.";
  strcpy(&new_kernel_data[0], new_data_string);

  // Write kernels to disk
  EXPECT_TRUE(utils::WriteFile(
      old_kernel.c_str(), &old_kernel_data[0], old_kernel_data.size()));
  EXPECT_TRUE(utils::WriteFile(
      new_kernel.c_str(), &new_kernel_data[0], new_kernel_data.size()));

  string delta_path;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/delta.XXXXXX", &delta_path, NULL));
  LOG(INFO) << "delta path: " << delta_path;
  ScopedPathUnlinker delta_path_unlinker(delta_path);
  {
    string a_mnt, b_mnt;
    ScopedLoopMounter a_mounter(a_img, &a_mnt, MS_RDONLY);
    ScopedLoopMounter b_mounter(b_img, &b_mnt, MS_RDONLY);
    const string private_key = kUnittestPrivateKeyPath;
    EXPECT_TRUE(
        DeltaDiffGenerator::GenerateDeltaUpdateFile(
            full_rootfs ? "" : a_mnt,
            full_rootfs ? "" : a_img,
            b_mnt,
            b_img,
            full_kernel ? "" : old_kernel,
            new_kernel,
            delta_path,
            private_key));
  }

  // Read delta into memory.
  vector<char> delta;
  uint64_t metadata_size;
  // Check the metadata.
  {
    DeltaArchiveManifest manifest;
    EXPECT_TRUE(PayloadSigner::LoadPayload(delta_path, &delta, &manifest,
                                           &metadata_size));

    LOG(INFO) << "Metadata size: " << metadata_size;

    {
      EXPECT_TRUE(manifest.has_signatures_offset());
      EXPECT_TRUE(manifest.has_signatures_size());
      Signatures sigs_message;
      EXPECT_TRUE(sigs_message.ParseFromArray(
          &delta[metadata_size + manifest.signatures_offset()],
          manifest.signatures_size()));
      EXPECT_EQ(1, sigs_message.signatures_size());
      const Signatures_Signature& signature = sigs_message.signatures(0);
      EXPECT_EQ(1, signature.version());

      uint64_t expected_sig_data_length = 0;
      vector<string> key_paths (1, kUnittestPrivateKeyPath);
      EXPECT_TRUE(PayloadSigner::SignatureBlobLength(
          key_paths,
          &expected_sig_data_length));
      EXPECT_EQ(expected_sig_data_length, manifest.signatures_size());
      EXPECT_FALSE(signature.data().empty());
    }

    if (full_kernel) {
      EXPECT_FALSE(manifest.has_old_kernel_info());
    } else {
      EXPECT_EQ(old_kernel_data.size(), manifest.old_kernel_info().size());
      EXPECT_FALSE(manifest.old_kernel_info().hash().empty());
    }

    if (full_rootfs) {
      EXPECT_FALSE(manifest.has_old_rootfs_info());
    } else {
      EXPECT_EQ(image_size, manifest.old_rootfs_info().size());
      EXPECT_FALSE(manifest.old_rootfs_info().hash().empty());
    }

    EXPECT_EQ(new_kernel_data.size(), manifest.new_kernel_info().size());
    EXPECT_EQ(image_size, manifest.new_rootfs_info().size());

    EXPECT_FALSE(manifest.new_kernel_info().hash().empty());
    EXPECT_FALSE(manifest.new_rootfs_info().hash().empty());

  }

  PrefsMock prefs;
  EXPECT_CALL(prefs, SetInt64(kPrefsManifestMetadataSize,
                              metadata_size)).WillOnce(Return(true));
  EXPECT_CALL(prefs, SetInt64(kPrefsUpdateStateNextOperation, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(prefs, GetInt64(kPrefsUpdateStateNextOperation, _))
      .WillOnce(Return(false));
  EXPECT_CALL(prefs, SetInt64(kPrefsUpdateStateNextDataOffset, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(prefs, SetString(kPrefsUpdateStateSHA256Context, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(prefs, SetString(kPrefsUpdateStateSignedSHA256Context, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(prefs, SetString(kPrefsUpdateStateSignatureBlob, _))
      .WillOnce(Return(true));

  // Update the A image in place.
  InstallPlan install_plan;
  install_plan.metadata_size = metadata_size;
  LOG(INFO) << "Setting payload metadata size in Omaha  = " << metadata_size;
  ASSERT_TRUE(PayloadSigner::GetMetadataSignature(
      &delta[0],
      metadata_size,
      kUnittestPrivateKeyPath,
      &install_plan.metadata_signature));
  EXPECT_FALSE(install_plan.metadata_signature.empty());

  MockSystemState mock_system_state;
  DeltaPerformer performer(&prefs, &mock_system_state, &install_plan);
  EXPECT_TRUE(utils::FileExists(kUnittestPublicKeyPath));
  performer.set_public_key_path(kUnittestPublicKeyPath);

  EXPECT_EQ(image_size,
            OmahaHashCalculator::RawHashOfFile(a_img,
                                               image_size,
                                               &install_plan.rootfs_hash));
  EXPECT_TRUE(OmahaHashCalculator::RawHashOfData(old_kernel_data,
                                                 &install_plan.kernel_hash));

  EXPECT_EQ(0, performer.Open(a_img.c_str(), 0, 0));
  EXPECT_TRUE(performer.OpenKernel(old_kernel.c_str()));

  // Write at some number of bytes per operation. Arbitrarily chose 5.
  const size_t kBytesPerWrite = 5;
  for (size_t i = 0; i < delta.size(); i += kBytesPerWrite) {
    size_t count = min(delta.size() - i, kBytesPerWrite);
    EXPECT_TRUE(performer.Write(&delta[i], count));
  }

  // Wrapper around close. Returns 0 on success or -errno on error.
  EXPECT_EQ(0, performer.Close());

  CompareFilesByBlock(old_kernel, new_kernel);
  CompareFilesByBlock(a_img, b_img);

  vector<char> updated_kernel_partition;
  EXPECT_TRUE(utils::ReadFile(old_kernel, &updated_kernel_partition));
  EXPECT_EQ(0, strncmp(&updated_kernel_partition[0], new_data_string,
                       strlen(new_data_string)));

  ActionExitCode expect_verify_result = kActionCodeSuccess;
  LOG(INFO) << "Verifying Payload ...";
  EXPECT_EQ(expect_verify_result, performer.VerifyPayload(
      OmahaHashCalculator::OmahaHashOfData(delta),
      delta.size()));

  LOG(INFO) << "Verified Payload";

  uint64_t new_kernel_size;
  vector<char> new_kernel_hash;
  uint64_t new_rootfs_size;
  vector<char> new_rootfs_hash;
  EXPECT_TRUE(performer.GetNewPartitionInfo(&new_kernel_size,
                                            &new_kernel_hash,
                                            &new_rootfs_size,
                                            &new_rootfs_hash));
  EXPECT_EQ(4096, new_kernel_size);
  vector<char> expected_new_kernel_hash;
  EXPECT_TRUE(OmahaHashCalculator::RawHashOfData(new_kernel_data,
                                                 &expected_new_kernel_hash));
  EXPECT_TRUE(expected_new_kernel_hash == new_kernel_hash);
  EXPECT_EQ(image_size, new_rootfs_size);
  vector<char> expected_new_rootfs_hash;
  EXPECT_EQ(image_size,
            OmahaHashCalculator::RawHashOfFile(b_img,
                                               image_size,
                                               &expected_new_rootfs_hash));
  EXPECT_TRUE(expected_new_rootfs_hash == new_rootfs_hash);
}
}  // namespace {}

TEST(DeltaPerformerTest, RunAsRootSmallImageTest) {
  DoSmallImageTest(false, false, false, kSignatureGenerator);
}

TEST(DeltaPerformerTest, RunAsRootFullKernelSmallImageTest) {
  DoSmallImageTest(true, false, false, kSignatureGenerator);
}

TEST(DeltaPerformerTest, RunAsRootFullSmallImageTest) {
  DoSmallImageTest(true, true, false, kSignatureGenerator);
}

TEST(DeltaPerformerTest, RunAsRootNoopSmallImageTest) {
  DoSmallImageTest(false, false, true, kSignatureGenerator);
}

TEST(DeltaPerformerTest, RunAsRootSmallImageSignNoneTest) {
  DoSmallImageTest(false, false, false, kSignatureNone);
}

TEST(DeltaPerformerTest, RunAsRootSmallImageSignGeneratedTest) {
  DoSmallImageTest(false, false, false, kSignatureGenerated);
}

TEST(DeltaPerformerTest, RunAsRootSmallImageSignGeneratedShellTest) {
  DoSmallImageTest(false, false, false, kSignatureGeneratedShell);
}

TEST(DeltaPerformerTest, RunAsRootSmallImageSignGeneratedShellBadKeyTest) {
  DoSmallImageTest(false, false, false, kSignatureGeneratedShellBadKey);
}

TEST(DeltaPerformerTest, RunAsRootSmallImageSignGeneratedShellRotateCl1Test) {
  DoSmallImageTest(false, false, false, kSignatureGeneratedShellRotateCl1);
}

TEST(DeltaPerformerTest, RunAsRootSmallImageSignGeneratedShellRotateCl2Test) {
  DoSmallImageTest(false, false, false, kSignatureGeneratedShellRotateCl2);
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

TEST(DeltaPerformerTest, RunAsRootRunValidManifestTest) {
  DoManifestTest();
}


}  // namespace chromeos_update_engine
