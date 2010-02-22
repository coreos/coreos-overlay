// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>
#include <gtest/gtest.h>
#include "update_engine/graph_utils.h"

using std::make_pair;
using std::vector;

namespace chromeos_update_engine {

class GraphUtilsTest : public ::testing::Test {};

TEST(GraphUtilsTest, SimpleTest) {
  Graph graph(2);
  
  graph[0].out_edges.insert(make_pair(1, EdgeProperties()));

  vector<Extent>& extents = graph[0].out_edges[1].extents;

  EXPECT_EQ(0, extents.size());
  graph_utils::AppendBlockToExtents(&extents, 0);
  EXPECT_EQ(1, extents.size());
  graph_utils::AppendBlockToExtents(&extents, 1);
  graph_utils::AppendBlockToExtents(&extents, 2);
  EXPECT_EQ(1, extents.size());
  graph_utils::AppendBlockToExtents(&extents, 4);
  
  EXPECT_EQ(2, extents.size());
  EXPECT_EQ(0, extents[0].start_block());
  EXPECT_EQ(3, extents[0].num_blocks());
  EXPECT_EQ(4, extents[1].start_block());
  EXPECT_EQ(1, extents[1].num_blocks());
  
  EXPECT_EQ(4, graph_utils::EdgeWeight(graph, make_pair(0, 1)));
}

}  // namespace chromeos_update_engine
