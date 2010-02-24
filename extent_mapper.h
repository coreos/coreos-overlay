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

bool ExtentsForFileFibmap(const std::string& path, std::vector<Extent>* out);

}  // namespace extent_mapper

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_EXTENT_MAPPER_H__
