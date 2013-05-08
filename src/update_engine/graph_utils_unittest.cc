// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "update_engine/graph_utils.h"
#include "update_engine/extent_ranges.h"

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

TEST(GraphUtilsTest, AppendSparseToExtentsTest) {
  vector<Extent> extents;

  EXPECT_EQ(0, extents.size());
  graph_utils::AppendBlockToExtents(&extents, kSparseHole);
  EXPECT_EQ(1, extents.size());
  graph_utils::AppendBlockToExtents(&extents, 0);
  EXPECT_EQ(2, extents.size());
  graph_utils::AppendBlockToExtents(&extents, kSparseHole);
  graph_utils::AppendBlockToExtents(&extents, kSparseHole);

  ASSERT_EQ(3, extents.size());
  EXPECT_EQ(kSparseHole, extents[0].start_block());
  EXPECT_EQ(1, extents[0].num_blocks());
  EXPECT_EQ(0, extents[1].start_block());
  EXPECT_EQ(1, extents[1].num_blocks());
  EXPECT_EQ(kSparseHole, extents[2].start_block());
  EXPECT_EQ(2, extents[2].num_blocks());
}

TEST(GraphUtilsTest, BlocksInExtentsTest) {
  {
    vector<Extent> extents;
    EXPECT_EQ(0, graph_utils::BlocksInExtents(extents));
    extents.push_back(ExtentForRange(0, 1));
    EXPECT_EQ(1, graph_utils::BlocksInExtents(extents));
    extents.push_back(ExtentForRange(23, 55));
    EXPECT_EQ(56, graph_utils::BlocksInExtents(extents));
    extents.push_back(ExtentForRange(1, 2));
    EXPECT_EQ(58, graph_utils::BlocksInExtents(extents));
  }
  {
    google::protobuf::RepeatedPtrField<Extent> extents;
    EXPECT_EQ(0, graph_utils::BlocksInExtents(extents));
    *extents.Add() = ExtentForRange(0, 1);
    EXPECT_EQ(1, graph_utils::BlocksInExtents(extents));
    *extents.Add() = ExtentForRange(23, 55);
    EXPECT_EQ(56, graph_utils::BlocksInExtents(extents));
    *extents.Add() = ExtentForRange(1, 2);
    EXPECT_EQ(58, graph_utils::BlocksInExtents(extents));
  }
}

TEST(GraphUtilsTest, DepsTest) {
  Graph graph(3);

  graph_utils::AddReadBeforeDep(&graph[0], 1, 3);
  EXPECT_EQ(1, graph[0].out_edges.size());
  {
    Extent& extent = graph[0].out_edges[1].extents[0];
    EXPECT_EQ(3, extent.start_block());
    EXPECT_EQ(1, extent.num_blocks());
  }
  graph_utils::AddReadBeforeDep(&graph[0], 1, 4);
  EXPECT_EQ(1, graph[0].out_edges.size());
  {
    Extent& extent = graph[0].out_edges[1].extents[0];
    EXPECT_EQ(3, extent.start_block());
    EXPECT_EQ(2, extent.num_blocks());
  }
  graph_utils::AddReadBeforeDepExtents(&graph[2], 1,
    vector<Extent>(1, ExtentForRange(5, 2)));
  EXPECT_EQ(1, graph[2].out_edges.size());
  {
    Extent& extent = graph[2].out_edges[1].extents[0];
    EXPECT_EQ(5, extent.start_block());
    EXPECT_EQ(2, extent.num_blocks());
  }
  // Change most recent edge from read-before to write-before
  graph[2].out_edges[1].write_extents.swap(graph[2].out_edges[1].extents);
  graph_utils::DropWriteBeforeDeps(&graph[2].out_edges);
  EXPECT_EQ(0, graph[2].out_edges.size());

  EXPECT_EQ(1, graph[0].out_edges.size());
  graph_utils::DropIncomingEdgesTo(&graph, 1);
  EXPECT_EQ(0, graph[0].out_edges.size());
}

}  // namespace chromeos_update_engine
