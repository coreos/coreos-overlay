// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_GRAPH_UTILS_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_GRAPH_UTILS_H__

#include <vector>
#include "base/basictypes.h"
#include "update_engine/graph_types.h"
#include "update_engine/update_metadata.pb.h"

// A few utility functions for graphs

namespace chromeos_update_engine {

namespace graph_utils {

// Returns the number of blocks represented by all extents in the edge.
uint64 EdgeWeight(const Graph& graph, const Edge& edge);

// block must either be the next block in the last extent or a block
// in the next extent. This function will not handle inserting block
// into an arbitrary place in the extents.
void AppendBlockToExtents(std::vector<Extent>* extents, uint64 block);

}  // namespace graph_utils

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_GRAPH_UTILS_H__
