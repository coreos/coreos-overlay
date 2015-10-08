// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <glog/logging.h>
#include <gtest/gtest.h>
#include "update_engine/graph_types.h"
#include "update_engine/tarjan.h"
#include "update_engine/utils.h"

using std::make_pair;
using std::pair;
using std::set;
using std::string;
using std::vector;

namespace chromeos_update_engine {

class TarjanAlgorithmTest : public ::testing::Test {};

TEST(TarjanAlgorithmTest, SimpleTest) {
  const Vertex::Index n_a = 0;
  const Vertex::Index n_b = 1;
  const Vertex::Index n_c = 2;
  const Vertex::Index n_d = 3;
  const Vertex::Index n_e = 4;
  const Vertex::Index n_f = 5;
  const Vertex::Index n_g = 6;
  const Vertex::Index n_h = 7;
  const Graph::size_type kNodeCount = 8;

  Graph graph(kNodeCount);
  
  graph[n_a].out_edges.insert(make_pair(n_e, EdgeProperties()));
  graph[n_a].out_edges.insert(make_pair(n_f, EdgeProperties()));
  graph[n_b].out_edges.insert(make_pair(n_a, EdgeProperties()));
  graph[n_c].out_edges.insert(make_pair(n_d, EdgeProperties()));
  graph[n_d].out_edges.insert(make_pair(n_e, EdgeProperties()));
  graph[n_d].out_edges.insert(make_pair(n_f, EdgeProperties()));
  graph[n_e].out_edges.insert(make_pair(n_b, EdgeProperties()));
  graph[n_e].out_edges.insert(make_pair(n_c, EdgeProperties()));
  graph[n_e].out_edges.insert(make_pair(n_f, EdgeProperties()));
  graph[n_f].out_edges.insert(make_pair(n_g, EdgeProperties()));
  graph[n_g].out_edges.insert(make_pair(n_h, EdgeProperties()));
  graph[n_h].out_edges.insert(make_pair(n_g, EdgeProperties()));
  
  TarjanAlgorithm tarjan;
  
  for (Vertex::Index i = n_a; i <= n_e; i++) {
    vector<Vertex::Index> vertex_indexes;
    tarjan.Execute(i, &graph, &vertex_indexes);

    EXPECT_EQ(5, vertex_indexes.size());
    EXPECT_TRUE(utils::VectorContainsValue(vertex_indexes, n_a));
    EXPECT_TRUE(utils::VectorContainsValue(vertex_indexes, n_b));
    EXPECT_TRUE(utils::VectorContainsValue(vertex_indexes, n_c));
    EXPECT_TRUE(utils::VectorContainsValue(vertex_indexes, n_d));
    EXPECT_TRUE(utils::VectorContainsValue(vertex_indexes, n_e));
  }
  
  {
    vector<Vertex::Index> vertex_indexes;
    tarjan.Execute(n_f, &graph, &vertex_indexes);
    
    EXPECT_EQ(1, vertex_indexes.size());
    EXPECT_TRUE(utils::VectorContainsValue(vertex_indexes, n_f));
  }
  
  for (Vertex::Index i = n_g; i <= n_h; i++) {
    vector<Vertex::Index> vertex_indexes;
    tarjan.Execute(i, &graph, &vertex_indexes);

    EXPECT_EQ(2, vertex_indexes.size());
    EXPECT_TRUE(utils::VectorContainsValue(vertex_indexes, n_g));
    EXPECT_TRUE(utils::VectorContainsValue(vertex_indexes, n_h));
  }
}

}  // namespace chromeos_update_engine
