// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <utility>
#include <vector>
#include <glog/logging.h>
#include <gtest/gtest.h>
#include "update_engine/cycle_breaker.h"
#include "update_engine/graph_types.h"
#include "update_engine/utils.h"

using std::make_pair;
using std::pair;
using std::set;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
void SetOpForNodes(Graph* graph) {
  for (Graph::iterator it = graph->begin(), e = graph->end(); it != e; ++it) {
    it->op.set_type(InstallOperation_Type_MOVE);
  }
}
}  // namespace {}

class CycleBreakerTest : public ::testing::Test {};

TEST(CycleBreakerTest, SimpleTest) {
  int counter = 0;
  const Vertex::Index n_a = counter++;
  const Vertex::Index n_b = counter++;
  const Vertex::Index n_c = counter++;
  const Vertex::Index n_d = counter++;
  const Vertex::Index n_e = counter++;
  const Vertex::Index n_f = counter++;
  const Vertex::Index n_g = counter++;
  const Vertex::Index n_h = counter++;
  const Graph::size_type kNodeCount = counter++;

  Graph graph(kNodeCount);
  SetOpForNodes(&graph);

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

  CycleBreaker breaker;
  
  set<Edge> broken_edges;
  breaker.BreakCycles(graph, &broken_edges);
  
  // The following cycles must be cut:
  // A->E->B
  // C->D->E
  // G->H

  EXPECT_TRUE(broken_edges.count(make_pair(n_a, n_e)) ||
              broken_edges.count(make_pair(n_e, n_b)) ||
              broken_edges.count(make_pair(n_b, n_a)));
  EXPECT_TRUE(broken_edges.count(make_pair(n_c, n_d)) ||
              broken_edges.count(make_pair(n_d, n_e)) ||
              broken_edges.count(make_pair(n_e, n_c)));
  EXPECT_TRUE(broken_edges.count(make_pair(n_g, n_h)) ||
              broken_edges.count(make_pair(n_h, n_g)));
  EXPECT_EQ(3, broken_edges.size());
}

namespace {
pair<Vertex::Index, EdgeProperties> EdgeWithWeight(Vertex::Index dest,
uint64_t weight) {
  EdgeProperties props;
  props.extents.resize(1);
  props.extents[0].set_num_blocks(weight);
  return make_pair(dest, props);
}
}  // namespace {}


// This creates a bunch of cycles like this:
//
//               root <------.
//    (t)->     / | \        |
//             V  V  V       |
//             N  N  N       |
//              \ | /        |
//               VVV         |
//                N          |
//              / | \        |
//             V  V  V       |
//             N  N  N       |
//               ...         |
//     (s)->    \ | /        |
//               VVV         |
//                N          |
//                 \_________/
//
// such that the original cutting algo would cut edges (s). We changed
// the algorithm to cut cycles (t) instead, since they are closer to the
// root, and that can massively speed up cycle cutting.
TEST(CycleBreakerTest, AggressiveCutTest) {
  int counter = 0;
  
  const int kNodesPerGroup = 4;
  const int kGroups = 33;
  
  Graph graph(kGroups * kNodesPerGroup + 1);  // + 1 for the root node
  SetOpForNodes(&graph);
  
  const Vertex::Index n_root = counter++;

  Vertex::Index last_hub = n_root;
  for (int i = 0; i < kGroups; i++) {
    uint64_t weight = 5;
    if (i == 0)
      weight = 2;
    else if (i == (kGroups - 1))
      weight = 1;
    
    const Vertex::Index next_hub = counter++;

    for (int j = 0; j < (kNodesPerGroup - 1); j++) {
      const Vertex::Index node = counter++;
      graph[last_hub].out_edges.insert(EdgeWithWeight(node, weight));
      graph[node].out_edges.insert(EdgeWithWeight(next_hub, weight));
    }
    last_hub = next_hub;
  }
  
  graph[last_hub].out_edges.insert(EdgeWithWeight(n_root, 5));
  
  
  EXPECT_EQ(counter, graph.size());

  CycleBreaker breaker;
  
  set<Edge> broken_edges;
  LOG(INFO) << "If this hangs for more than 1 second, the test has failed.";
  breaker.BreakCycles(graph, &broken_edges);
  
  set<Edge> expected_cuts;

  for (Vertex::EdgeMap::const_iterator it = graph[n_root].out_edges.begin(),
       e = graph[n_root].out_edges.end(); it != e; ++it) {
    expected_cuts.insert(make_pair(n_root, it->first));
  }

  EXPECT_TRUE(broken_edges == expected_cuts);
}

TEST(CycleBreakerTest, WeightTest) {
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
  SetOpForNodes(&graph);

  graph[n_a].out_edges.insert(EdgeWithWeight(n_b, 4));
  graph[n_a].out_edges.insert(EdgeWithWeight(n_f, 3));
  graph[n_a].out_edges.insert(EdgeWithWeight(n_h, 2));
  graph[n_b].out_edges.insert(EdgeWithWeight(n_a, 3));
  graph[n_b].out_edges.insert(EdgeWithWeight(n_c, 4));
  graph[n_c].out_edges.insert(EdgeWithWeight(n_b, 5));
  graph[n_c].out_edges.insert(EdgeWithWeight(n_d, 3));
  graph[n_d].out_edges.insert(EdgeWithWeight(n_a, 6));
  graph[n_d].out_edges.insert(EdgeWithWeight(n_e, 3));
  graph[n_e].out_edges.insert(EdgeWithWeight(n_d, 4));
  graph[n_e].out_edges.insert(EdgeWithWeight(n_g, 5));
  graph[n_f].out_edges.insert(EdgeWithWeight(n_g, 2));
  graph[n_g].out_edges.insert(EdgeWithWeight(n_f, 3));
  graph[n_g].out_edges.insert(EdgeWithWeight(n_d, 5));
  graph[n_h].out_edges.insert(EdgeWithWeight(n_i, 8));
  graph[n_i].out_edges.insert(EdgeWithWeight(n_e, 4));
  graph[n_i].out_edges.insert(EdgeWithWeight(n_h, 9));
  graph[n_i].out_edges.insert(EdgeWithWeight(n_j, 6));

  CycleBreaker breaker;
  
  set<Edge> broken_edges;
  breaker.BreakCycles(graph, &broken_edges);

  // These are required to be broken:
  EXPECT_TRUE(broken_edges.count(make_pair(n_b, n_a)));
  EXPECT_TRUE(broken_edges.count(make_pair(n_b, n_c)));
  EXPECT_TRUE(broken_edges.count(make_pair(n_d, n_e)));
  EXPECT_TRUE(broken_edges.count(make_pair(n_f, n_g)));
  EXPECT_TRUE(broken_edges.count(make_pair(n_h, n_i)));
}

TEST(CycleBreakerTest, UnblockGraphTest) {
  int counter = 0;
  const Vertex::Index n_a = counter++;
  const Vertex::Index n_b = counter++;
  const Vertex::Index n_c = counter++;
  const Vertex::Index n_d = counter++;
  const Graph::size_type kNodeCount = counter++;

  Graph graph(kNodeCount);
  SetOpForNodes(&graph);

  graph[n_a].out_edges.insert(EdgeWithWeight(n_b, 1));
  graph[n_a].out_edges.insert(EdgeWithWeight(n_c, 1));
  graph[n_b].out_edges.insert(EdgeWithWeight(n_c, 2));
  graph[n_c].out_edges.insert(EdgeWithWeight(n_b, 2));
  graph[n_b].out_edges.insert(EdgeWithWeight(n_d, 2));
  graph[n_d].out_edges.insert(EdgeWithWeight(n_a, 2));

  CycleBreaker breaker;
  
  set<Edge> broken_edges;
  breaker.BreakCycles(graph, &broken_edges);

  // These are required to be broken:
  EXPECT_TRUE(broken_edges.count(make_pair(n_a, n_b)));
  EXPECT_TRUE(broken_edges.count(make_pair(n_a, n_c)));
}

TEST(CycleBreakerTest, SkipOpsTest) {
  int counter = 0;
  const Vertex::Index n_a = counter++;
  const Vertex::Index n_b = counter++;
  const Vertex::Index n_c = counter++;
  const Graph::size_type kNodeCount = counter++;

  Graph graph(kNodeCount);
  SetOpForNodes(&graph);
  graph[n_a].op.set_type(InstallOperation_Type_REPLACE_BZ);
  graph[n_c].op.set_type(InstallOperation_Type_REPLACE);

  graph[n_a].out_edges.insert(EdgeWithWeight(n_b, 1));
  graph[n_c].out_edges.insert(EdgeWithWeight(n_b, 1));

  CycleBreaker breaker;
  
  set<Edge> broken_edges;
  breaker.BreakCycles(graph, &broken_edges);
  
  EXPECT_EQ(2, breaker.skipped_ops());
}

}  // namespace chromeos_update_engine
