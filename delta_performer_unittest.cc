// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mount.h>
#include <inttypes.h>

#include <algorithm>
#include <string>
#include <vector>

#include <base/scoped_ptr.h>
#include <base/string_util.h>
#include <google/protobuf/repeated_field.h>
#include <gtest/gtest.h>

#include "update_engine/delta_diff_generator.h"
#include "update_engine/delta_performer.h"
#include "update_engine/graph_types.h"
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

class ScopedLoopMounter {
 public:
  explicit ScopedLoopMounter(const string& file_path, string* mnt_path,
                             unsigned long flags) {
    EXPECT_TRUE(utils::MakeTempDirectory("/tmp/mnt.XXXXXX", mnt_path));
    dir_remover_.reset(new ScopedDirRemover(*mnt_path));

    string loop_dev = GetUnusedLoopDevice();
    EXPECT_EQ(0, system(StringPrintf("losetup %s %s", loop_dev.c_str(),
                                     file_path.c_str()).c_str()));
    loop_releaser_.reset(new ScopedLoopbackDeviceReleaser(loop_dev));

    EXPECT_TRUE(utils::MountFilesystem(loop_dev, *mnt_path, flags));
    unmounter_.reset(new ScopedFilesystemUnmounter(*mnt_path));
  }
 private:
  scoped_ptr<ScopedDirRemover> dir_remover_;
  scoped_ptr<ScopedLoopbackDeviceReleaser> loop_releaser_;
  scoped_ptr<ScopedFilesystemUnmounter> unmounter_;
};

void CompareFilesByBlock(const string& a_file, const string& b_file) {
  vector<char> a_data, b_data;
  EXPECT_TRUE(utils::ReadFile(a_file, &a_data)) << "file failed: " << a_file;
  EXPECT_TRUE(utils::ReadFile(b_file, &b_data)) << "file failed: " << b_file;

  EXPECT_EQ(a_data.size(), b_data.size());
  size_t kBlockSize = 4096;
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
}

TEST(DeltaPerformerTest, RunAsRootSmallImageTest) {
  string a_img, b_img;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/a_img.XXXXXX", &a_img, NULL));
  ScopedPathUnlinker a_img_unlinker(a_img);
  EXPECT_TRUE(utils::MakeTempFile("/tmp/b_img.XXXXXX", &b_img, NULL));
  ScopedPathUnlinker b_img_unlinker(b_img);

  CreateExtImageAtPath(a_img, NULL);
  CreateExtImageAtPath(b_img, NULL);

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

  // Make some changes to the B image.
  {
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

    EXPECT_TRUE(
        DeltaDiffGenerator::GenerateDeltaUpdateFile(a_mnt,
                                                    a_img,
                                                    b_mnt,
                                                    b_img,
                                                    old_kernel,
                                                    new_kernel,
                                                    delta_path,
                                                    kUnittestPrivateKeyPath));
  }

  // Read delta into memory.
  vector<char> delta;
  EXPECT_TRUE(utils::ReadFile(delta_path, &delta));

  uint64_t manifest_metadata_size;

  // Check that the null signature blob exists
  {
    LOG(INFO) << "delta size: " << delta.size();
    DeltaArchiveManifest manifest;
    const int kManifestSizeOffset = 12;
    const int kManifestOffset = 20;
    uint64_t manifest_size = 0;
    memcpy(&manifest_size, &delta[kManifestSizeOffset], sizeof(manifest_size));
    manifest_size = be64toh(manifest_size);
    LOG(INFO) << "manifest size: " << manifest_size;
    EXPECT_TRUE(manifest.ParseFromArray(&delta[kManifestOffset],
                                        manifest_size));
    EXPECT_TRUE(manifest.has_signatures_offset());
    manifest_metadata_size = kManifestOffset + manifest_size;

    Signatures sigs_message;
    EXPECT_TRUE(sigs_message.ParseFromArray(
        &delta[manifest_metadata_size + manifest.signatures_offset()],
        manifest.signatures_size()));
    EXPECT_EQ(1, sigs_message.signatures_size());
    const Signatures_Signature& signature = sigs_message.signatures(0);
    EXPECT_EQ(1, signature.version());

    uint64_t expected_sig_data_length = 0;
    EXPECT_TRUE(PayloadSigner::SignatureBlobLength(kUnittestPrivateKeyPath,
                                                   &expected_sig_data_length));
    EXPECT_EQ(expected_sig_data_length, manifest.signatures_size());
    EXPECT_FALSE(signature.data().empty());
  }

  PrefsMock prefs;
  EXPECT_CALL(prefs, SetInt64(kPrefsManifestMetadataSize,
                              manifest_metadata_size)).WillOnce(Return(true));
  EXPECT_CALL(prefs, SetInt64(kPrefsUpdateStateNextOperation, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(prefs, SetInt64(kPrefsUpdateStateNextDataOffset, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(prefs, SetString(kPrefsUpdateStateSignedSHA256Context, _))
      .WillRepeatedly(Return(true));

  // Update the A image in place.
  DeltaPerformer performer(&prefs);

  EXPECT_EQ(0, performer.Open(a_img.c_str(), 0, 0));
  EXPECT_TRUE(performer.OpenKernel(old_kernel.c_str()));

  // Write at some number of bytes per operation. Arbitrarily chose 5.
  const size_t kBytesPerWrite = 5;
  for (size_t i = 0; i < delta.size(); i += kBytesPerWrite) {
    size_t count = min(delta.size() - i, kBytesPerWrite);
    EXPECT_EQ(count, performer.Write(&delta[i], count));
  }

  // Wrapper around close. Returns 0 on success or -errno on error.
  EXPECT_EQ(0, performer.Close());

  CompareFilesByBlock(old_kernel, new_kernel);

  vector<char> updated_kernel_partition;
  EXPECT_TRUE(utils::ReadFile(old_kernel, &updated_kernel_partition));
  EXPECT_EQ(0, strncmp(&updated_kernel_partition[0], new_data_string,
                       strlen(new_data_string)));

  EXPECT_TRUE(utils::FileExists(kUnittestPublicKeyPath));
  EXPECT_TRUE(performer.VerifyPayload(kUnittestPublicKeyPath));
}

}  // namespace chromeos_update_engine
