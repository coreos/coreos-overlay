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
#include "update_engine/install_plan.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

class PrefsInterface;

// This class performs the actions in a delta update synchronously. The delta
// update itself should be passed in in chunks as it is received.

class DeltaPerformer : public FileWriter {
 public:

  static const char kUpdatePayloadPublicKeyPath[];

  DeltaPerformer(PrefsInterface* prefs,
                 InstallPlan* install_plan)
      : prefs_(prefs),
        install_plan_(install_plan),
        fd_(-1),
        manifest_valid_(false),
        manifest_metadata_size_(0),
        next_operation_num_(0),
        buffer_offset_(0),
        last_updated_buffer_offset_(kuint64max),
        block_size_(0),
        public_key_path_(kUpdatePayloadPublicKeyPath),
        num_rootfs_operations_(0),
        num_total_operations_(0) {}

  // Once Close()d, a DeltaPerformer can't be Open()ed again.
  int Open();

  // FileWriter's Write implementation where caller doesn't care about
  // error codes.
  bool Write(const void* bytes, size_t count) {
    ActionExitCode error;
    return Write(bytes, count, &error);
  }

  // FileWriter's Write implementation that returns a more specific |error| code
  // in case of failures in Write operation.
  bool Write(const void* bytes, size_t count, ActionExitCode *error);

  // Wrapper around close. Returns 0 on success or -errno on error.
  int Close();

  // Verifies the downloaded payload against the signed hash included in the
  // payload, against the update check hash (which is in base64 format)  and
  // size using the public key and returns kActionCodeSuccess on success, an
  // error code on failure.  This method should be called after closing the
  // stream. Note this method skips the signed hash check if the public key is
  // unavailable; it returns kActionCodeSignedDeltaPayloadExpectedError if the
  // public key is available but the delta payload doesn't include a signature.
  ActionExitCode VerifyPayload(const std::string& update_check_response_hash,
                               const uint64_t update_check_response_size);

  // Reads from the update manifest the expected size and hash of the target
  // rootfs partition. These values can be used for applied update
  // hash verification. This method must be called after the update manifest has
  // been parsed (e.g., after closing the stream). Returns true on success, and
  // false on failure (e.g., when the values are not present in the update
  // manifest).
  bool GetNewPartitionInfo(uint64_t* rootfs_size,
                           std::vector<char>* rootfs_hash);

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

  void set_public_key_path(const std::string& public_key_path) {
    public_key_path_ = public_key_path;
  }

 private:
  friend class DeltaPerformerTest;
  FRIEND_TEST(DeltaPerformerTest, IsIdempotentOperationTest);

  static bool IsIdempotentOperation(
      const InstallOperation& op);

  // Verifies that the expected source partition hash (if present) match the
  // hash for the current partition. Returns true if there're no expected
  // hash in the payload (e.g., if it's a new-style full update) or if the
  // hashes match; returns false otherwise.
  bool VerifySourcePartition();

  // Returns true if enough of the delta file has been passed via Write()
  // to be able to perform a given install operation.
  bool CanPerformInstallOperation(
      const InstallOperation& operation);

  // Validates that the hash of the blobs corresponding to the given |operation|
  // matches what's specified in the manifest in the payload.
  // Returns kActionCodeSuccess on match or a suitable error code otherwise.
  ActionExitCode ValidateOperationHash(
      const InstallOperation& operation);

  // Returns true on success.
  bool PerformInstallOperation(
      const InstallOperation& operation);

  // These perform a specific type of operation and return true on success.
  bool PerformReplaceOperation(
      const InstallOperation& operation);
  bool PerformMoveOperation(
      const InstallOperation& operation);
  bool PerformBsdiffOperation(
      const InstallOperation& operation);

  // Returns true if the payload signature message has been extracted from
  // |operation|, false otherwise.
  bool ExtractSignatureMessage(
      const InstallOperation& operation);

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

  // Install Plan based on Omaha Response.
  InstallPlan* install_plan_;

  // File descriptor of open device.
  int fd_;

  DeltaArchiveManifest manifest_;
  bool manifest_valid_;
  uint64_t manifest_metadata_size_;

  // Index of the next operation to perform in the manifest.
  size_t next_operation_num_;

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

  // The public key to be used. Provided as a member so that tests can
  // override with test keys.
  std::string public_key_path_;

  // The number rootfs and total operations in a payload, once we know them.
  size_t num_rootfs_operations_;
  size_t num_total_operations_;

  DISALLOW_COPY_AND_ASSIGN(DeltaPerformer);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_PERFORMER_H__
