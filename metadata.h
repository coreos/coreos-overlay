// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_METADATA_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_METADATA_H__

#include "update_engine/delta_diff_generator.h"
#include "update_engine/graph_types.h"

namespace chromeos_update_engine {

class Metadata {
 public:
  // Reads metadata from old image and new image and determines
  // the smallest way to encode the metadata for the diff.
  // If there's no change in the metadata, it creates a MOVE
  // operation. If there is a change, the smallest of REPLACE, REPLACE_BZ,
  // or BSDIFF wins. It writes the diff to data_fd and updates data_file_size
  // accordingly. It also adds the required operation to the graph and adds the
  // metadata extents to blocks.
  // Returns true on success.
  static bool DeltaReadMetadata(Graph* graph,
                                std::vector<DeltaDiffGenerator::Block>* blocks,
                                const std::string& old_image,
                                const std::string& new_image,
                                int data_fd,
                                off_t* data_file_size);

 private:
  // This should never be constructed.
  DISALLOW_IMPLICIT_CONSTRUCTORS(Metadata);
};

};  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_METADATA_H__
