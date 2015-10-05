// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_GRAPH_UTILS_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_GRAPH_UTILS_H__

#include <vector>

#include "update_engine/graph_types.h"
#include "update_engine/update_metadata.pb.h"

// A few utility functions for graphs

namespace chromeos_update_engine {

namespace graph_utils {

// Returns the number of blocks represented by all extents in the edge.
uint64_t EdgeWeight(const Graph& graph, const Edge& edge);

// These add a read-before dependency from graph[src] -> graph[dst]. If the dep
// already exists, the block/s is/are added to the existing edge.
void AddReadBeforeDep(Vertex* src,
                      Vertex::Index dst,
                      uint64_t block);
void AddReadBeforeDepExtents(Vertex* src,
                             Vertex::Index dst,
                             const std::vector<Extent>& extents);

void DropWriteBeforeDeps(Vertex::EdgeMap* edge_map);

// For each node N in graph, drop all edges N->|index|.
void DropIncomingEdgesTo(Graph* graph, Vertex::Index index);

// block must either be the next block in the last extent or a block
// in the next extent. This function will not handle inserting block
// into an arbitrary place in the extents.
void AppendBlockToExtents(std::vector<Extent>* extents, uint64_t block);

// Get/SetElement are intentionally overloaded so that templated functions
// can accept either type of collection of Extents.
Extent GetElement(const std::vector<Extent>& collection, size_t index);
Extent GetElement(
    const google::protobuf::RepeatedPtrField<Extent>& collection,
    size_t index);

template<typename T>
uint64_t BlocksInExtents(const T& collection) {
  uint64_t ret = 0;
  for (size_t i = 0; i < static_cast<size_t>(collection.size()); ++i) {
    ret += GetElement(collection, i).num_blocks();
  }
  return ret;
}

void DumpGraph(const Graph& graph);

}  // namespace graph_utils

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_GRAPH_UTILS_H__
