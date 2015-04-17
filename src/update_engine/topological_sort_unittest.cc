// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>
#include <gtest/gtest.h>
#include "update_engine/graph_types.h"
#include "update_engine/topological_sort.h"

using std::make_pair;
using std::vector;

namespace chromeos_update_engine {

class TopologicalSortTest : public ::testing::Test {};

namespace {
// Returns true if the value is found in vect. If found, the index is stored
// in out_index if out_index is not null.
template<typename T>
bool IndexOf(const vector<T>& vect,
             const T& value,
             typename vector<T>::size_type* out_index) {
  for (typename vector<T>::size_type i = 0; i < vect.size(); i++) {
    if (vect[i] == value) {
      if (out_index) {
        *out_index = i;
      }
      return true;
    }
  }
  return false;
}
}  // namespace {}

TEST(TopologicalSortTest, SimpleTest) {
  int counter = 0;
  const Vertex::Index n_a = counter++;
  const Vertex::Index n_b = counter++;
  const Vertex::Index n_c = counter++;
  const Vertex::Index n_d = counter++;
  const Vertex::Index n_e = counter++;
  const Vertex::Index n_f = counter++;
  const Vertex::Index n_g = counter++;
  const Vertex::Index n_h = counter++;
  const Vertex::Index n_i = counter++;
  const Vertex::Index n_j = counter++;
  const Graph::size_type kNodeCount = counter++;

  Graph graph(kNodeCount);
  
  graph[n_i].out_edges.insert(make_pair(n_j, EdgeProperties()));
  graph[n_i].out_edges.insert(make_pair(n_c, EdgeProperties()));
  graph[n_i].out_edges.insert(make_pair(n_e, EdgeProperties()));
  graph[n_i].out_edges.insert(make_pair(n_h, EdgeProperties()));
  graph[n_c].out_edges.insert(make_pair(n_b, EdgeProperties()));
  graph[n_b].out_edges.insert(make_pair(n_a, EdgeProperties()));
  graph[n_e].out_edges.insert(make_pair(n_d, EdgeProperties()));
  graph[n_e].out_edges.insert(make_pair(n_g, EdgeProperties()));
  graph[n_g].out_edges.insert(make_pair(n_d, EdgeProperties()));
  graph[n_g].out_edges.insert(make_pair(n_f, EdgeProperties()));
  graph[n_d].out_edges.insert(make_pair(n_a, EdgeProperties()));

  vector<Vertex::Index> sorted;
  TopologicalSort(graph, &sorted);

  for (Vertex::Index i = 0; i < graph.size(); i++) {
    vector<Vertex::Index>::size_type src_index = 0;
    EXPECT_TRUE(IndexOf(sorted, i, &src_index));
    for (Vertex::EdgeMap::const_iterator it = graph[i].out_edges.begin();
         it != graph[i].out_edges.end(); ++it) {
      vector<Vertex::Index>::size_type dst_index = 0;
      EXPECT_TRUE(IndexOf(sorted, it->first, &dst_index));
      EXPECT_LT(dst_index, src_index);
    }
  }
}

}  // namespace chromeos_update_engine
