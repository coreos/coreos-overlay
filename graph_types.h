// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_GRAPH_TYPES_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_GRAPH_TYPES_H__

#include <map>
#include <set>
#include <utility>
#include <vector>
#include "base/basictypes.h"
#include "update_engine/update_metadata.pb.h"

// A few classes that help in generating delta images use these types
// for the graph work.

namespace chromeos_update_engine {

struct EdgeProperties {
  std::vector<Extent> extents;  // filesystem extents represented
};

struct Vertex {
  Vertex() : index(-1), lowlink(-1), op(NULL) {}
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
  DeltaArchiveManifest_InstallOperation* op;

  typedef std::vector<Vertex>::size_type Index;
  static const Vertex::Index kInvalidIndex = -1;
};

typedef std::vector<Vertex> Graph;

typedef std::pair<Vertex::Index, Vertex::Index> Edge;

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_GRAPH_TYPES_H__
