// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_TOPOLOGICAL_SORT_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_TOPOLOGICAL_SORT_H__


#include <vector>
#include "update_engine/graph_types.h"

namespace chromeos_update_engine {

// Performs a topological sort on the directed graph 'graph' and stores
// the nodes, in order visited, in 'out'.
// For example, this graph:
// A ---> C ----.
//  \           v
//   `--> B --> D
// Might result in this in 'out':
// out[0] = D
// out[1] = B
// out[2] = C
// out[3] = A
// Note: results are undefined if there is a cycle in the graph.
void TopologicalSort(const Graph& graph, std::vector<Vertex::Index>* out);

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_TOPOLOGICAL_SORT_H__
