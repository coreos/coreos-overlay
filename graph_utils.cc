// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/graph_utils.h"
#include "base/basictypes.h"

using std::vector;

namespace chromeos_update_engine {

namespace graph_utils {

uint64 EdgeWeight(const Graph& graph, const Edge& edge) {
  uint64 weight = 0;
  const vector<Extent>& extents =
      graph[edge.first].out_edges.find(edge.second)->second.extents;
  for (vector<Extent>::const_iterator it = extents.begin();
       it != extents.end(); ++it) {
    weight += it->num_blocks();
  }
  return weight;
}

void AppendBlockToExtents(vector<Extent>* extents, uint64 block) {
  if (!extents->empty()) {
    Extent& extent = extents->back();
    if (extent.start_block() + extent.num_blocks() == block) {
      extent.set_num_blocks(extent.num_blocks() + 1);
      return;
    }
  }
  Extent new_extent;
  new_extent.set_start_block(block);
  new_extent.set_num_blocks(1);
  extents->push_back(new_extent);
}

}  // namespace graph_utils

}  // namespace chromeos_update_engine
