// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/topological_sort.h"
#include <set>
#include <vector>
#include <glog/logging.h>

using std::set;
using std::vector;

namespace chromeos_update_engine {

namespace {
void TopologicalSortVisit(const Graph& graph,
                          set<Vertex::Index>* visited_nodes,
                          vector<Vertex::Index>* nodes,
                          Vertex::Index node) {
  if (visited_nodes->find(node) != visited_nodes->end())
    return;

  visited_nodes->insert(node);
  // Visit all children.
  for (Vertex::EdgeMap::const_iterator it = graph[node].out_edges.begin();
       it != graph[node].out_edges.end(); ++it) {
    TopologicalSortVisit(graph, visited_nodes, nodes, it->first);
  }
  // Visit this node.
  nodes->push_back(node);
}
}  // namespace {}

void TopologicalSort(const Graph& graph, vector<Vertex::Index>* out) {
  set<Vertex::Index> visited_nodes;

  for (Vertex::Index i = 0; i < graph.size(); i++) {
    TopologicalSortVisit(graph, &visited_nodes, out, i);
  }
}

}  // namespace chromeos_update_engine
