// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_PERFORMER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_PERFORMER_H__

#include <inttypes.h>

#include <vector>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/action_processor.h"
#include "update_engine/install_plan.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

class PrefsInterface;

// This class performs the actions in a delta update synchronously. The delta
// update itself should be passed in in chunks as it is received.

class DeltaPerformer {
 public:

  DeltaPerformer(PrefsInterface* prefs,
                 InstallPlan* install_plan)
      : prefs_(prefs),
        install_plan_(install_plan),
        fd_(-1),
        block_size_(0) {}

  // Once Close()d, a DeltaPerformer can't be Open()ed again.
  int Open();

  // Processes a single operation on the target partition.
  ActionExitCode PerformOperation(const InstallOperation& operation,
                                  const std::vector<char>& data);

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

  // Set block size specified by the manifest.
  void SetBlockSize(uint32_t size) {
    block_size_ = size;
  }

 private:
  friend class DeltaPerformerTest;
  FRIEND_TEST(DeltaPerformerTest, ExtentsToByteStringTest);
  FRIEND_TEST(DeltaPerformerTest, IsIdempotentOperationTest);

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

  static bool IsIdempotentOperation(
      const InstallOperation& op);

  // Validates that the hash of the blobs corresponding to the given |operation|
  // matches what's specified in the manifest in the payload.
  // Returns kActionCodeSuccess on match or a suitable error code otherwise.
  ActionExitCode ValidateOperationHash(const InstallOperation& operation,
                                       const std::vector<char>& data);

  // These perform a specific type of operation and return true on success.
  bool PerformReplaceOperation(const InstallOperation& operation,
                               const std::vector<char>& data);
  bool PerformMoveOperation(const InstallOperation& operation);
  bool PerformBsdiffOperation(const InstallOperation& operation,
                              const std::vector<char>& data);

  // Update Engine preference store.
  PrefsInterface* prefs_;

  // Install Plan based on Omaha Response.
  InstallPlan* install_plan_;

  // File descriptor of open device.
  int fd_;

  // The block size (parsed from the manifest).
  uint32_t block_size_;

  DISALLOW_COPY_AND_ASSIGN(DeltaPerformer);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_PERFORMER_H__
