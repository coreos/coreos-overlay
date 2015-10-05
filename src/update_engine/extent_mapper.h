// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_EXTENT_MAPPER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_EXTENT_MAPPER_H__

#include <string>
#include <vector>

#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

namespace extent_mapper {

// Uses the FIBMAP ioctl to get all blocks used by a file and return them
// as extents. Blocks are relative to the start of the filesystem. If
// there is a sparse "hole" in the file, the blocks for that will be
// represented by an extent whose start block is kSpareseHole.
// The resulting extents are stored in 'out'. Keep in mind that while
// the blocksize of a filesystem is often 4096 bytes, that is not always
// the case, so one should consult GetFilesystemBlockSize(), too.
// Returns true on success.
bool ExtentsForFileFibmap(const std::string& path, std::vector<Extent>* out);

// Puts the blocksize of the filesystem, as used by the FIBMAP ioctl, into
// out_blocksize by using the FIGETBSZ ioctl. Returns true on success.
bool GetFilesystemBlockSize(const std::string& path, uint32_t* out_blocksize);

}  // namespace extent_mapper

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_EXTENT_MAPPER_H__
