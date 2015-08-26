// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Copyright (c) 2015 CoreOS, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_PROCESSOR_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_PROCESSOR_H__

#include <inttypes.h>

#include <limits>

#include "update_engine/delta_performer.h"
#include "update_engine/file_writer.h"
#include "update_engine/install_plan.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

class PrefsInterface;
class SystemState;

// This class processes an update payload, dispatching install operations
// in the manifest to FileWriters as the payload data is received.

class PayloadProcessor : public FileWriter {
 public:

  static const char kUpdatePayloadPublicKeyPath[];

  PayloadProcessor(PrefsInterface* prefs, InstallPlan* install_plan)
      : delta_performer_(prefs, install_plan->install_path),
        prefs_(prefs),
        install_plan_(install_plan),
        manifest_valid_(false),
        manifest_metadata_size_(0),
        next_operation_num_(0),
        buffer_offset_(0),
        last_updated_buffer_offset_(std::numeric_limits<uint64_t>::max()),
        public_key_path_(kUpdatePayloadPublicKeyPath),
        num_total_operations_(0) {}

  // Once Close()d, a PayloadProcessor can't be Open()ed again.
  int Open() {
    return delta_performer_.Open();
  }

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
  // partition. These values can be used for applied update hash verification.
  // This method must be called after the update manifest has been parsed
  // (e.g., after closing the stream). Returns true on success, and false on
  // failure (e.g., when the values are not present in the update manifest).
  bool GetNewPartitionInfo(uint64_t* partition_size,
                           std::vector<char>* partition_hash);

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
  // Parses the manifest and finishes any initialization that needs info from
  // the manifest. Result may be kActionCodeDownloadIncomplete.
  ActionExitCode LoadManifest();

  // Verifies that the expected source partition hash (if present) match the
  // hash for the current partition. Returns true if there're no expected
  // hash in the payload (e.g., if it's a new-style full update) or if the
  // hashes match; returns false otherwise.
  bool VerifySourcePartition();

  // Returns true if the payload signature message has been extracted from
  // payload, false otherwise.
  bool ExtractSignatureMessage(const std::vector<char>& data);

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

  // Writer for the main partition to be updated.
  DeltaPerformer delta_performer_;

  // Update Engine preference store.
  PrefsInterface* prefs_;

  // Install Plan based on Omaha Response.
  InstallPlan* install_plan_;

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

  // Calculates the payload hash.
  OmahaHashCalculator hash_calculator_;

  // Saves the signed hash context.
  std::string signed_hash_context_;

  // Signatures message blob extracted directly from the payload.
  std::vector<char> signatures_message_data_;

  // The public key to be used. Provided as a member so that tests can
  // override with test keys.
  std::string public_key_path_;

  // The number of total operations in a payload, once we know them.
  size_t num_total_operations_;

  DISALLOW_COPY_AND_ASSIGN(PayloadProcessor);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_PROCESSOR_H__
