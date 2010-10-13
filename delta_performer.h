// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_PERFORMER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_PERFORMER_H__

#include <inttypes.h>

#include <vector>

#include <google/protobuf/repeated_field.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/file_writer.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

class PrefsInterface;

// This class performs the actions in a delta update synchronously. The delta
// update itself should be passed in in chunks as it is received.

class DeltaPerformer : public FileWriter {
 public:
  DeltaPerformer(PrefsInterface* prefs)
      : prefs_(prefs),
        fd_(-1),
        kernel_fd_(-1),
        manifest_valid_(false),
        manifest_metadata_size_(0),
        next_operation_num_(0),
        buffer_offset_(0),
        last_updated_buffer_offset_(kuint64max),
        block_size_(0),
        current_kernel_hash_(NULL),
        current_rootfs_hash_(NULL) {}

  // Opens the kernel. Should be called before or after Open(), but before
  // Write(). The kernel file will be close()d when Close() is called.
  bool OpenKernel(const char* kernel_path);

  // flags and mode ignored. Once Close()d, a DeltaPerformer can't be
  // Open()ed again.
  int Open(const char* path, int flags, mode_t mode);

  // Wrapper around write. Returns bytes written on success or
  // -errno on error.
  ssize_t Write(const void* bytes, size_t count);

  // Wrapper around close. Returns 0 on success or -errno on error.
  // Closes both 'path' given to Open() and the kernel path.
  int Close();

  // Verifies the downloaded payload against the signed hash included in the
  // payload as well as against the update check hash and size and returns true
  // on success, false on failure. This method should be called after closing
  // the stream. Note this method skips the signed hash check if the public key
  // is unavailable; it returns false if the public key is available but the
  // delta payload doesn't include a signature. If |public_key_path| is an empty
  // string, uses the default public key path.
  bool VerifyPayload(const std::string& public_key_path,
                     const std::string& update_check_response_hash,
                     const uint64_t update_check_response_size);

  // Verifies that the generated update is correct based on the hashes sent by
  // the server. Returns true on success, false otherwise.
  bool VerifyAppliedUpdate(const std::string& path,
                           const std::string& kernel_path);

  // Converts an ordered collection of Extent objects which contain data of
  // length full_length to a comma-separated string. For each Extent, the
  // string will have the start offset and then the length in bytes.
  // The length value of the last extent in the string may be short, since
  // the full length of all extents in the string is capped to full_length.
  // Also, an extent starting at kSparseHole, appears as -1 in the string.
  // For example, if the Extents are {1, 1}, {4, 2}, {kSparseHole, 1},
  // {0, 1}, block_size is 4096, and full_length is 5 * block_size - 13,
  // the resulting string will be: "4096:4096,16384:8192,-1:4096,0:4083"
  static bool ExtentsToBsdiffPositionsString(
      const google::protobuf::RepeatedPtrField<Extent>& extents,
      uint64_t block_size,
      uint64_t full_length,
      std::string* positions_string);

  // Returns true if a previous update attempt can be continued based on the
  // persistent preferences and the new update check response hash.
  static bool CanResumeUpdate(PrefsInterface* prefs,
                              std::string update_check_response_hash);

  // Resets the persistent update progress state to indicate that an update
  // can't be resumed. Performs a quick update-in-progress reset if |quick| is
  // true, otherwise resets all progress-related update state. Returns true on
  // success, false otherwise.
  static bool ResetUpdateProgress(PrefsInterface* prefs, bool quick);

  void set_current_kernel_hash(const std::vector<char>* hash) {
    current_kernel_hash_ = hash;
  }

  void set_current_rootfs_hash(const std::vector<char>* hash) {
    current_rootfs_hash_ = hash;
  }

 private:
  friend class DeltaPerformerTest;
  FRIEND_TEST(DeltaPerformerTest, IsIdempotentOperationTest);

  static bool IsIdempotentOperation(
      const DeltaArchiveManifest_InstallOperation& op);

  // Verifies that the expected source partition hashes (if present) match the
  // hashes for the current partitions. Returns true if there're no expected
  // hashes in the payload (e.g., if it's a new-style full update) or if the
  // hashes match; returns false otherwise.
  bool VerifySourcePartitions();

  // Returns true if enough of the delta file has been passed via Write()
  // to be able to perform a given install operation.
  bool CanPerformInstallOperation(
      const DeltaArchiveManifest_InstallOperation& operation);

  // Returns true on success.
  bool PerformInstallOperation(
      const DeltaArchiveManifest_InstallOperation& operation);

  // These perform a specific type of operation and return true on success.
  bool PerformReplaceOperation(
      const DeltaArchiveManifest_InstallOperation& operation,
      bool is_kernel_partition);
  bool PerformMoveOperation(
      const DeltaArchiveManifest_InstallOperation& operation,
      bool is_kernel_partition);
  bool PerformBsdiffOperation(
      const DeltaArchiveManifest_InstallOperation& operation,
      bool is_kernel_partition);

  // Returns true if the payload signature message has been extracted from
  // |operation|, false otherwise.
  bool ExtractSignatureMessage(
      const DeltaArchiveManifest_InstallOperation& operation);

  // Updates the hash calculator with |count| bytes at the head of |buffer_| and
  // then discards them.
  void DiscardBufferHeadBytes(size_t count);

  // Checkpoints the update progress into persistent storage to allow this
  // update attempt to be resumed after reboot.
  bool CheckpointUpdateProgress();

  // Primes the required update state. Returns true if the update state was
  // successfully initialized to a saved resume state or if the update is a new
  // update. Returns false otherwise.
  bool PrimeUpdateState();

  // Update Engine preference store.
  PrefsInterface* prefs_;

  // File descriptor of open device.
  int fd_;

  // File descriptor of the kernel device
  int kernel_fd_;

  std::string path_;  // Path that fd_ refers to.
  std::string kernel_path_;  // Path that kernel_fd_ refers to.

  DeltaArchiveManifest manifest_;
  bool manifest_valid_;
  uint64_t manifest_metadata_size_;

  // Index of the next operation to perform in the manifest.
  int next_operation_num_;

  // buffer_ is a window of the data that's been downloaded. At first,
  // it contains the beginning of the download, but after the protobuf
  // has been downloaded and parsed, it contains a sliding window of
  // data blobs.
  std::vector<char> buffer_;
  // Offset of buffer_ in the binary blobs section of the update.
  uint64_t buffer_offset_;

  // Last |buffer_offset_| value updated as part of the progress update.
  uint64_t last_updated_buffer_offset_;

  // The block size (parsed from the manifest).
  uint32_t block_size_;

  // Calculates the payload hash.
  OmahaHashCalculator hash_calculator_;

  // Saves the signed hash context.
  std::string signed_hash_context_;

  // Signatures message blob extracted directly from the payload.
  std::vector<char> signatures_message_data_;

  // Hashes for the current partitions to be used for source partition
  // verification.
  const std::vector<char>* current_kernel_hash_;
  const std::vector<char>* current_rootfs_hash_;

  DISALLOW_COPY_AND_ASSIGN(DeltaPerformer);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_PERFORMER_H__
