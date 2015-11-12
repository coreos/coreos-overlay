// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_METADATA_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_METADATA_H__

#include <inttypes.h>

#include <vector>

#include <update_engine/action_processor.h>
#include <update_engine/update_metadata.pb.h>

namespace chromeos_update_engine {

// Update payload header field values and sizes.
// See update_metadata.proto for details.
extern const char kDeltaMagic[];
const uint64_t kDeltaVersion = 1;

const uint64_t kDeltaMagicSize = 4;
const uint64_t kDeltaVersionSize = sizeof(uint64_t);
const uint64_t kDeltaManifestSizeSize = sizeof(uint64_t);

const uint64_t kDeltaVersionOffset = kDeltaMagicSize;
const uint64_t kDeltaManifestSizeOffset = kDeltaVersionOffset + kDeltaVersionSize;
const uint64_t kDeltaManifestOffset = kDeltaManifestSizeOffset + kDeltaManifestSizeSize;

class DeltaMetadata {
 public:
  // Attempts to parse the update metadata starting from the beginning of
  // |payload| into |manifest|. On success, sets |metadata_size| to the total
  // metadata bytes (including the delta magic and metadata size fields), and
  // returns kActionCodeSuccess. Returns kActionCodeDownloadIncomplete if more
  // data is needed to parse the complete metadata. Returns
  // kActionCodeDownloadManifestParseError if the metadata can't be parsed.
  static ActionExitCode ParsePayload(
      const std::vector<char>& payload,
      DeltaArchiveManifest* manifest,
      uint64_t* metadata_size);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(DeltaMetadata);
};

};  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_METADATA_H__
