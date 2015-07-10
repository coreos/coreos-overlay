// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_FULL_UPDATE_GENERATOR_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_FULL_UPDATE_GENERATOR_H__

#include <glib.h>

#include "update_engine/graph_types.h"

namespace chromeos_update_engine {

class FullUpdateGenerator {
 public:
  // Reads a new rootfs (|new_image|), creating a full update of chunk_size
  // chunks. Populates |graph| and |final_order| with data about the update
  // operations, and writes relevant data to |fd|, updating |data_file_size| as
  // it does. Only the first |image_size| bytes are read from |new_image|
  // assuming that this is the actual file system.
  static bool Run(
      Graph* graph,
      const std::string& new_image,
      off_t image_size,
      int fd,
      off_t* data_file_size,
      off_t chunk_size,
      off_t block_size,
      std::vector<Vertex::Index>* final_order);

 private:
  // This should never be constructed.
  DISALLOW_IMPLICIT_CONSTRUCTORS(FullUpdateGenerator);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_FULL_UPDATE_GENERATOR_H__
