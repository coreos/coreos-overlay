// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_GRAPH_TYPES_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_GRAPH_TYPES_H__

#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "macros.h"
#include "update_engine/update_metadata.pb.h"

// A few classes that help in generating delta images use these types
// for the graph work.

namespace chromeos_update_engine {

bool operator==(const Extent& a, const Extent& b);

struct EdgeProperties {
  // Read-before extents. I.e., blocks in |extents| must be read by the
  // node pointed to before the pointing node runs (presumably b/c it
  // overwrites these blocks).
  std::vector<Extent> extents;
  
  // Write before extents. I.e., blocks in |write_extents| must be written
  // by the node pointed to before the pointing node runs (presumably
  // b/c it reads the data written by the other node).
  std::vector<Extent> write_extents;
  
  bool operator==(const EdgeProperties& that) const {
    return extents == that.extents && write_extents == that.write_extents;
  }
};

struct Vertex {
  Vertex() : valid(true), index(-1), lowlink(-1) {}
  bool valid;
  
  typedef std::map<std::vector<Vertex>::size_type, EdgeProperties> EdgeMap;
  EdgeMap out_edges;

  // We sometimes wish to consider a subgraph of a graph. A subgraph would have
  // a subset of the vertices from the graph and a subset of the edges.
  // When considering this vertex within a subgraph, subgraph_edges stores
  // the out-edges.
  typedef std::set<std::vector<Vertex>::size_type> SubgraphEdgeMap;
  SubgraphEdgeMap subgraph_edges;

  // For Tarjan's algorithm:
  std::vector<Vertex>::size_type index;
  std::vector<Vertex>::size_type lowlink;

  // Other Vertex properties:
  InstallOperation op;
  std::string file_name;

  typedef std::vector<Vertex>::size_type Index;
  static const Vertex::Index kInvalidIndex = -1;
};

typedef std::vector<Vertex> Graph;

typedef std::pair<Vertex::Index, Vertex::Index> Edge;

const uint64_t kSparseHole = std::numeric_limits<uint64_t>::max();
const uint64_t kTempBlockStart = 1ULL << 60;
static_assert(kTempBlockStart != 0, "kTempBlockStart invalid");

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_GRAPH_TYPES_H__
