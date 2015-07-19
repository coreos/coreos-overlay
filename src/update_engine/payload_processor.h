// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Copyright (c) 2015 CoreOS, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_PROCESSOR_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_PROCESSOR_H__

#include <inttypes.h>

#include "update_engine/delta_performer.h"
#include "update_engine/file_writer.h"

namespace chromeos_update_engine {

class PrefsInterface;
class SystemState;
class InstallPlan;

// This class processes an update payload, dispatching install operations
// in the manifest to FileWriters as the payload data is received.

class PayloadProcessor : public FileWriter {
 public:

  PayloadProcessor(PrefsInterface* prefs, InstallPlan* install_plan)
      : delta_performer_(prefs, install_plan) {}

  // flags and mode ignored. Once Close()d, a PayloadProcessor can't be
  // Open()ed again.
  int Open(const char* path, int flags, mode_t mode) {
    return delta_performer_.Open(path, flags, mode);
  }

  // FileWriter's Write implementation where caller doesn't care about
  // error codes.
  bool Write(const void* bytes, size_t count) {
    ActionExitCode error;
    return Write(bytes, count, &error);
  }

  // FileWriter's Write implementation that returns a more specific |error| code
  // in case of failures in Write operation.
  bool Write(const void* bytes, size_t count, ActionExitCode *error) {
    return delta_performer_.Write(bytes, count, error);
  }

  // Wrapper around close. Returns 0 on success or -errno on error.
  int Close() {
    return delta_performer_.Close();
  }

  // Verifies the downloaded payload against the signed hash included in the
  // payload, against the update check hash (which is in base64 format)  and
  // size using the public key and returns kActionCodeSuccess on success, an
  // error code on failure.  This method should be called after closing the
  // stream. Note this method skips the signed hash check if the public key is
  // unavailable; it returns kActionCodeSignedDeltaPayloadExpectedError if the
  // public key is available but the delta payload doesn't include a signature.
  ActionExitCode VerifyPayload(const std::string& update_check_response_hash,
                               const uint64_t update_check_response_size) {
    return delta_performer_.VerifyPayload(update_check_response_hash,
                                          update_check_response_size);
  }

  // Reads from the update manifest the expected size and hash of the target
  // partition. These values can be used for applied update hash verification.
  // This method must be called after the update manifest has been parsed
  // (e.g., after closing the stream). Returns true on success, and false on
  // failure (e.g., when the values are not present in the update manifest).
  bool GetNewPartitionInfo(uint64_t* partition_size,
                           std::vector<char>* partition_hash) {
    return delta_performer_.GetNewPartitionInfo(partition_size, partition_hash);
  }

 private:

  // Writer for the man partition to be updated.
  DeltaPerformer delta_performer_;

  DISALLOW_COPY_AND_ASSIGN(PayloadProcessor);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_PROCESSOR_H__
