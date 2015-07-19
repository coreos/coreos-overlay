// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_METADATA_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_METADATA_H__

#include <inttypes.h>

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

};  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_METADATA_H__
