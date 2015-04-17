// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_PERFORMER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_PERFORMER_H__

#include <inttypes.h>

#include <vector>

#include <base/time.h>
#include <google/protobuf/repeated_field.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/file_writer.h"
#include "update_engine/install_plan.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/system_state.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

class PrefsInterface;

// This class performs the actions in a delta update synchronously. The delta
// update itself should be passed in in chunks as it is received.

class DeltaPerformer : public FileWriter {
 public:
  enum MetadataParseResult {
    kMetadataParseSuccess,
    kMetadataParseError,
    kMetadataParseInsufficientData,
  };

  static const uint64_t kDeltaVersionSize;
  static const uint64_t kDeltaManifestSizeSize;
  static const char kUpdatePayloadPublicKeyPath[];

  // Defines the granularity of progress logging in terms of how many "completed
  // chunks" we want to report at the most.
  static const unsigned kProgressLogMaxChunks;
  // Defines a timeout since the last progress was logged after which we want to
  // force another log message (even if the current chunk was not completed).
  static const unsigned kProgressLogTimeoutSeconds;
  // These define the relative weights (0-100) we give to the different work
  // components associated with an update when computing an overall progress.
  // Currently they include the download progress and the number of completed
  // operations. They must add up to one hundred (100).
  static const unsigned kProgressDownloadWeight;
  static const unsigned kProgressOperationsWeight;

  DeltaPerformer(PrefsInterface* prefs,
                 SystemState* system_state,
                 InstallPlan* install_plan)
      : prefs_(prefs),
        system_state_(system_state),
        install_plan_(install_plan),
        fd_(-1),
        kernel_fd_(-1),
        manifest_valid_(false),
        manifest_metadata_size_(0),
        next_operation_num_(0),
        buffer_offset_(0),
        last_updated_buffer_offset_(kuint64max),
        block_size_(0),
        public_key_path_(kUpdatePayloadPublicKeyPath),
        total_bytes_received_(0),
        num_rootfs_operations_(0),
        num_total_operations_(0),
        overall_progress_(0),
        last_progress_chunk_(0),
        forced_progress_log_wait_(
            base::TimeDelta::FromSeconds(kProgressLogTimeoutSeconds)) {}

  // Opens the kernel. Should be called before or after Open(), but before
  // Write(). The kernel file will be close()d when Close() is called.
  bool OpenKernel(const char* kernel_path);

  // flags and mode ignored. Once Close()d, a DeltaPerformer can't be
  // Open()ed again.
  int Open(const char* path, int flags, mode_t mode);

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
  // Closes both 'path' given to Open() and the kernel path.
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

  // Reads from the update manifest the expected sizes and hashes of the target
  // kernel and rootfs partitions. These values can be used for applied update
  // hash verification. This method must be called after the update manifest has
  // been parsed (e.g., after closing the stream). Returns true on success, and
  // false on failure (e.g., when the values are not present in the update
  // manifest).
  bool GetNewPartitionInfo(uint64_t* kernel_size,
                           std::vector<char>* kernel_hash,
                           uint64_t* rootfs_size,
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

  // Attempts to parse the update metadata starting from the beginning of
  // |payload| into |manifest|. On success, sets |metadata_size| to the total
  // metadata bytes (including the delta magic and metadata size fields), and
  // returns kMetadataParseSuccess. Returns kMetadataParseInsufficientData if
  // more data is needed to parse the complete metadata. Returns
  // kMetadataParseError if the metadata can't be parsed given the payload.
  MetadataParseResult ParsePayloadMetadata(
      const std::vector<char>& payload,
      DeltaArchiveManifest* manifest,
      uint64_t* metadata_size,
      ActionExitCode* error);

  void set_public_key_path(const std::string& public_key_path) {
    public_key_path_ = public_key_path;
  }

  // Returns the byte offset at which the manifest protobuf begins in a
  // payload.
  static uint64_t GetManifestOffset();

  // Returns the byte offset where the size of the manifest is stored in
  // a payload. This offset precedes the actual start of the manifest
  // that's returned by the GetManifestOffset method.
  static uint64_t GetManifestSizeOffset();

 private:
  friend class DeltaPerformerTest;
  FRIEND_TEST(DeltaPerformerTest, IsIdempotentOperationTest);

  // Logs the progress of downloading/applying an update.
  void LogProgress(const char* message_prefix);

  // Update overall progress metrics, log as necessary.
  void UpdateOverallProgress(bool force_log, const char* message_prefix);

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

  // Validates that the hash of the blobs corresponding to the given |operation|
  // matches what's specified in the manifest in the payload.
  // Returns kActionCodeSuccess on match or a suitable error code otherwise.
  ActionExitCode ValidateOperationHash(
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

  // Global context of the system.
  SystemState* system_state_;

  // Install Plan based on Omaha Response.
  InstallPlan* install_plan_;

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

  // The number of bytes received so far, used for progress tracking.
  size_t total_bytes_received_;

  // The number rootfs and total operations in a payload, once we know them.
  size_t num_rootfs_operations_;
  size_t num_total_operations_;

  // An overall progress counter, which should reflect both download progress
  // and the ratio of applied operations. Range is 0-100.
  unsigned overall_progress_;

  // The last progress chunk recorded.
  unsigned last_progress_chunk_;

  // The timeout after which we should force emitting a progress log (constant),
  // and the actual point in time for the next forced log to be emitted.
  const base::TimeDelta forced_progress_log_wait_;
  base::Time forced_progress_log_time_;

  DISALLOW_COPY_AND_ASSIGN(DeltaPerformer);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_PERFORMER_H__
