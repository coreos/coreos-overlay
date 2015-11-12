// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/delta_metadata.h"

#include <endian.h>
#include <string.h>

#include <glog/logging.h>

namespace chromeos_update_engine {

const char kDeltaMagic[] = "CrAU";
static_assert(kDeltaMagicSize == sizeof(kDeltaMagic) - 1, "invalid size");

ActionExitCode DeltaMetadata::ParsePayload(
    const std::vector<char>& payload,
    DeltaArchiveManifest* manifest,
    uint64_t* metadata_size) {

  if (payload.size() < kDeltaManifestOffset) {
    // Don't have enough bytes to even know the manifest size.
    return kActionCodeDownloadIncomplete;
  }

  // Validate the magic string.
  if (memcmp(payload.data(), kDeltaMagic, strlen(kDeltaMagic)) != 0) {
    LOG(ERROR) << "Bad payload format -- invalid delta magic.";
    return kActionCodeDownloadInvalidMetadataMagicString;
  }

  // TODO(jaysri): Compare the version number and skip unknown manifest
  // versions. We don't check the version at all today.

  // Next, parse the manifest size.
  uint64_t manifest_size;
  static_assert(sizeof(manifest_size) == kDeltaManifestSizeSize,
                "manifest_size size mismatch");
  memcpy(&manifest_size,
         &payload[kDeltaManifestSizeOffset],
         kDeltaManifestSizeSize);
  manifest_size = be64toh(manifest_size);  // switch big endian to host

  // We should wait for the full metadata to be read in before we can parse it.
  *metadata_size = kDeltaManifestOffset + manifest_size;
  if (payload.size() < *metadata_size) {
    return kActionCodeDownloadIncomplete;
  }

  // The metadata in |payload| is deemed valid. So, it's now safe to
  // parse the protobuf.
  if (!manifest->ParseFromArray(&payload[kDeltaManifestOffset], manifest_size)) {
    LOG(ERROR) << "Unable to parse manifest in update file.";
    return kActionCodeDownloadManifestParseError;
  }
  return kActionCodeSuccess;
}

};  // namespace chromeos_update_engine
