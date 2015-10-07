// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/graph_utils.h"

#include <string>
#include <utility>
#include <vector>

#include <base/basictypes.h>
#include <base/logging.h>

using std::make_pair;
using std::pair;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace graph_utils {

uint64_t EdgeWeight(const Graph& graph, const Edge& edge) {
  uint64_t weight = 0;
  const vector<Extent>& extents =
      graph[edge.first].out_edges.find(edge.second)->second.extents;
  for (vector<Extent>::const_iterator it = extents.begin();
       it != extents.end(); ++it) {
    if (it->start_block() != kSparseHole)
      weight += it->num_blocks();
  }
  return weight;
}

void AppendBlockToExtents(vector<Extent>* extents, uint64_t block) {
  // First try to extend the last extent in |extents|, if any.
  if (!extents->empty()) {
    Extent& extent = extents->back();
    uint64_t next_block = extent.start_block() == kSparseHole ?
        kSparseHole : extent.start_block() + extent.num_blocks();
    if (next_block == block) {
      extent.set_num_blocks(extent.num_blocks() + 1);
      return;
    }
  }
  // If unable to extend the last extent, append a new single-block extent.
  Extent new_extent;
  new_extent.set_start_block(block);
  new_extent.set_num_blocks(1);
  extents->push_back(new_extent);
}

void AddReadBeforeDep(Vertex* src,
                      Vertex::Index dst,
                      uint64_t block) {
  Vertex::EdgeMap::iterator edge_it = src->out_edges.find(dst);
  if (edge_it == src->out_edges.end()) {
    // Must create new edge
    pair<Vertex::EdgeMap::iterator, bool> result =
        src->out_edges.insert(make_pair(dst, EdgeProperties()));
    CHECK(result.second);
    edge_it = result.first;
  }
  AppendBlockToExtents(&edge_it->second.extents, block);
}

void AddReadBeforeDepExtents(Vertex* src,
                             Vertex::Index dst,
                             const vector<Extent>& extents) {
  // TODO(adlr): Be more efficient than adding each block individually.
  for (vector<Extent>::const_iterator it = extents.begin(), e = extents.end();
       it != e; ++it) {
    const Extent& extent = *it;
    for (uint64_t block = extent.start_block(),
             block_end = extent.start_block() + extent.num_blocks();
         block != block_end; ++block) {
      AddReadBeforeDep(src, dst, block);
    }
  }
}

void DropWriteBeforeDeps(Vertex::EdgeMap* edge_map) {
  // Specially crafted for-loop for the map-iterate-delete dance.
  for (Vertex::EdgeMap::iterator it = edge_map->begin();
       it != edge_map->end(); ) {
    if (!it->second.write_extents.empty())
      it->second.write_extents.clear();
    if (it->second.extents.empty()) {
      // Erase *it, as it contains no blocks
      edge_map->erase(it++);
    } else {
      ++it;
    }
  }
}

// For each node N in graph, drop all edges N->|index|.
void DropIncomingEdgesTo(Graph* graph, Vertex::Index index) {
  // This would be much more efficient if we had doubly-linked
  // edges in the graph.
  for (Graph::iterator it = graph->begin(), e = graph->end(); it != e; ++it) {
    it->out_edges.erase(index);
  }
}

Extent GetElement(const std::vector<Extent>& collection, size_t index) {
  return collection[index];
}
Extent GetElement(
    const google::protobuf::RepeatedPtrField<Extent>& collection,
    size_t index) {
  return collection.Get(index);
}

namespace {
template<typename T>
void DumpExtents(const T& field, int prepend_space_count) {
  string header(prepend_space_count, ' ');
  for (int i = 0, e = field.size(); i != e; ++i) {
    LOG(INFO) << header << "(" << GetElement(field, i).start_block() << ", "
              << GetElement(field, i).num_blocks() << ")";
  }
}

void DumpOutEdges(const Vertex::EdgeMap& out_edges) {
  for (Vertex::EdgeMap::const_iterator it = out_edges.begin(),
           e = out_edges.end(); it != e; ++it) {
    LOG(INFO) << "    " << it->first << " read-before:";
    DumpExtents(it->second.extents, 6);
    LOG(INFO) << "      write-before:";
    DumpExtents(it->second.write_extents, 6);
  }
}
}  // namespace {}

void DumpGraph(const Graph& graph) {
  LOG(INFO) << "Graph length: " << graph.size();
  for (Graph::size_type i = 0, e = graph.size(); i != e; ++i) {
    string type_str = "UNK";
    switch(graph[i].op.type()) {
      case InstallOperation_Type_BSDIFF:
        type_str = "BSDIFF";
        break;
      case InstallOperation_Type_MOVE:
        type_str = "MOVE";
        break;
      case InstallOperation_Type_REPLACE:
        type_str = "REPLACE";
        break;
      case InstallOperation_Type_REPLACE_BZ:
        type_str = "REPLACE_BZ";
        break;
    }
    LOG(INFO) << i 
              << (graph[i].valid ? "" : "-INV")
              << ": " << graph[i].file_name
              << ": " << type_str;
    LOG(INFO) << "  src_extents:";
    DumpExtents(graph[i].op.src_extents(), 4);
    LOG(INFO) << "  dst_extents:";
    DumpExtents(graph[i].op.dst_extents(), 4);
    LOG(INFO) << "  out edges:";
    DumpOutEdges(graph[i].out_edges);
  }
}

}  // namespace graph_utils

bool operator==(const Extent& a, const Extent& b) {
  return a.start_block() == b.start_block() && a.num_blocks() == b.num_blocks();
}

}  // namespace chromeos_update_engine
