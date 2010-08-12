// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <gtest/gtest.h>
#include "base/logging.h"
#include "update_engine/cycle_breaker.h"
#include "update_engine/delta_diff_generator.h"
#include "update_engine/delta_performer.h"
#include "update_engine/graph_types.h"
#include "update_engine/graph_utils.h"
#include "update_engine/subprocess.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::make_pair;
using std::set;
using std::string;
using std::vector;

namespace chromeos_update_engine {

typedef DeltaDiffGenerator::Block Block;

namespace {
int64_t BlocksInExtents(
    const google::protobuf::RepeatedPtrField<Extent>& extents) {
  int64_t ret = 0;
  for (int i = 0; i < extents.size(); i++) {
    ret += extents.Get(i).num_blocks();
  }
  return ret;
}
}  // namespace {}

class DeltaDiffGeneratorTest : public ::testing::Test {
 protected:
  const string old_path() { return "DeltaDiffGeneratorTest-old_path"; }
  const string new_path() { return "DeltaDiffGeneratorTest-new_path"; }
  virtual void TearDown() {
    unlink(old_path().c_str());
    unlink(new_path().c_str());
  }
};

TEST_F(DeltaDiffGeneratorTest, RunAsRootMoveSmallTest) {
  EXPECT_TRUE(utils::WriteFile(old_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString)));
  EXPECT_TRUE(utils::WriteFile(new_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString)));
  vector<char> data;
  DeltaArchiveManifest_InstallOperation op;
  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_path(),
                                                 new_path(),
                                                 &data,
                                                 &op));
  EXPECT_TRUE(data.empty());

  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_MOVE, op.type());
  EXPECT_FALSE(op.has_data_offset());
  EXPECT_FALSE(op.has_data_length());
  EXPECT_EQ(1, op.src_extents_size());
  EXPECT_EQ(sizeof(kRandomString), op.src_length());
  EXPECT_EQ(1, op.dst_extents_size());
  EXPECT_EQ(sizeof(kRandomString), op.dst_length());
  EXPECT_EQ(BlocksInExtents(op.src_extents()),
            BlocksInExtents(op.dst_extents()));
  EXPECT_EQ(1, BlocksInExtents(op.dst_extents()));
}

TEST_F(DeltaDiffGeneratorTest, RunAsRootBsdiffSmallTest) {
  EXPECT_TRUE(utils::WriteFile(old_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString) - 1));
  EXPECT_TRUE(utils::WriteFile(new_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString)));
  vector<char> data;
  DeltaArchiveManifest_InstallOperation op;
  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_path(),
                                                 new_path(),
                                                 &data,
                                                 &op));
  EXPECT_FALSE(data.empty());

  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_BSDIFF, op.type());
  EXPECT_FALSE(op.has_data_offset());
  EXPECT_FALSE(op.has_data_length());
  EXPECT_EQ(1, op.src_extents_size());
  EXPECT_EQ(sizeof(kRandomString) - 1, op.src_length());
  EXPECT_EQ(1, op.dst_extents_size());
  EXPECT_EQ(sizeof(kRandomString), op.dst_length());
  EXPECT_EQ(BlocksInExtents(op.src_extents()),
            BlocksInExtents(op.dst_extents()));
  EXPECT_EQ(1, BlocksInExtents(op.dst_extents()));
}

TEST_F(DeltaDiffGeneratorTest, RunAsRootReplaceSmallTest) {
  vector<char> new_data;
  for (int i = 0; i < 2; i++) {
    new_data.insert(new_data.end(),
                    kRandomString,
                    kRandomString + sizeof(kRandomString));
    EXPECT_TRUE(utils::WriteFile(new_path().c_str(),
                                 &new_data[0],
                                 new_data.size()));
    vector<char> data;
    DeltaArchiveManifest_InstallOperation op;
    EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_path(),
                                                   new_path(),
                                                   &data,
                                                   &op));
    EXPECT_FALSE(data.empty());

    EXPECT_TRUE(op.has_type());
    const DeltaArchiveManifest_InstallOperation_Type expected_type =
        (i == 0 ? DeltaArchiveManifest_InstallOperation_Type_REPLACE :
         DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ);
    EXPECT_EQ(expected_type, op.type());
    EXPECT_FALSE(op.has_data_offset());
    EXPECT_FALSE(op.has_data_length());
    EXPECT_EQ(0, op.src_extents_size());
    EXPECT_FALSE(op.has_src_length());
    EXPECT_EQ(1, op.dst_extents_size());
    EXPECT_EQ(new_data.size(), op.dst_length());
    EXPECT_EQ(1, BlocksInExtents(op.dst_extents()));
  }
}

namespace {
void AppendExtent(vector<Extent>* vect, uint64_t start, uint64_t length) {
  vect->resize(vect->size() + 1);
  vect->back().set_start_block(start);
  vect->back().set_num_blocks(length);
}
void OpAppendExtent(DeltaArchiveManifest_InstallOperation* op,
                    uint64_t start,
                    uint64_t length) {
  Extent* extent = op->add_src_extents();
  extent->set_start_block(start);
  extent->set_num_blocks(length);
}
}

TEST_F(DeltaDiffGeneratorTest, SubstituteBlocksTest) {
  vector<Extent> remove_blocks;
  AppendExtent(&remove_blocks, 3, 3);
  AppendExtent(&remove_blocks, 7, 1);
  vector<Extent> replace_blocks;
  AppendExtent(&replace_blocks, 10, 2);
  AppendExtent(&replace_blocks, 13, 2);
  DeltaArchiveManifest_InstallOperation op;
  OpAppendExtent(&op, 4, 3);
  OpAppendExtent(&op, kSparseHole, 4);  // Sparse hole in file
  OpAppendExtent(&op, 3, 1);
  OpAppendExtent(&op, 7, 3);
  
  DeltaDiffGenerator::SubstituteBlocks(&op, remove_blocks, replace_blocks);
  
  EXPECT_EQ(7, op.src_extents_size());
  EXPECT_EQ(11, op.src_extents(0).start_block());
  EXPECT_EQ(1, op.src_extents(0).num_blocks());
  EXPECT_EQ(13, op.src_extents(1).start_block());
  EXPECT_EQ(1, op.src_extents(1).num_blocks());
  EXPECT_EQ(6, op.src_extents(2).start_block());
  EXPECT_EQ(1, op.src_extents(2).num_blocks());
  EXPECT_EQ(kSparseHole, op.src_extents(3).start_block());
  EXPECT_EQ(4, op.src_extents(3).num_blocks());
  EXPECT_EQ(10, op.src_extents(4).start_block());
  EXPECT_EQ(1, op.src_extents(4).num_blocks());
  EXPECT_EQ(14, op.src_extents(5).start_block());
  EXPECT_EQ(1, op.src_extents(5).num_blocks());
  EXPECT_EQ(8, op.src_extents(6).start_block());
  EXPECT_EQ(2, op.src_extents(6).num_blocks());
}

TEST_F(DeltaDiffGeneratorTest, CutEdgesTest) {
  Graph graph;
  vector<Block> blocks(9);
  
  // Create nodes in graph
  {
    graph.resize(graph.size() + 1);
    graph.back().op.set_type(DeltaArchiveManifest_InstallOperation_Type_MOVE);
    // Reads from blocks 3, 5, 7
    vector<Extent> extents;
    graph_utils::AppendBlockToExtents(&extents, 3);
    graph_utils::AppendBlockToExtents(&extents, 5);
    graph_utils::AppendBlockToExtents(&extents, 7);
    DeltaDiffGenerator::StoreExtents(extents,
                                     graph.back().op.mutable_src_extents());
    blocks[3].reader = graph.size() - 1;
    blocks[5].reader = graph.size() - 1;
    blocks[7].reader = graph.size() - 1;
    
    // Writes to blocks 1, 2, 4
    extents.clear();
    graph_utils::AppendBlockToExtents(&extents, 1);
    graph_utils::AppendBlockToExtents(&extents, 2);
    graph_utils::AppendBlockToExtents(&extents, 4);
    DeltaDiffGenerator::StoreExtents(extents,
                                     graph.back().op.mutable_dst_extents());
    blocks[1].writer = graph.size() - 1;
    blocks[2].writer = graph.size() - 1;
    blocks[4].writer = graph.size() - 1;
  }
  {
    graph.resize(graph.size() + 1);
    graph.back().op.set_type(DeltaArchiveManifest_InstallOperation_Type_MOVE);
    // Reads from blocks 1, 2, 4
    vector<Extent> extents;
    graph_utils::AppendBlockToExtents(&extents, 1);
    graph_utils::AppendBlockToExtents(&extents, 2);
    graph_utils::AppendBlockToExtents(&extents, 4);
    DeltaDiffGenerator::StoreExtents(extents,
                                     graph.back().op.mutable_src_extents());
    blocks[1].reader = graph.size() - 1;
    blocks[2].reader = graph.size() - 1;
    blocks[4].reader = graph.size() - 1;
    
    // Writes to blocks 3, 5, 6
    extents.clear();
    graph_utils::AppendBlockToExtents(&extents, 3);
    graph_utils::AppendBlockToExtents(&extents, 5);
    graph_utils::AppendBlockToExtents(&extents, 6);
    DeltaDiffGenerator::StoreExtents(extents,
                                     graph.back().op.mutable_dst_extents());
    blocks[3].writer = graph.size() - 1;
    blocks[5].writer = graph.size() - 1;
    blocks[6].writer = graph.size() - 1;
  }
  
  // Create edges
  DeltaDiffGenerator::CreateEdges(&graph, blocks);
  
  // Find cycles
  CycleBreaker cycle_breaker;
  set<Edge> cut_edges;
  cycle_breaker.BreakCycles(graph, &cut_edges);

  EXPECT_EQ(1, cut_edges.size());
  EXPECT_TRUE(cut_edges.end() != cut_edges.find(make_pair<Vertex::Index>(1,
                                                                         0)));

  EXPECT_TRUE(DeltaDiffGenerator::CutEdges(&graph, blocks, cut_edges));
  
  EXPECT_EQ(3, graph.size());
  
  // Check new node in graph:
  EXPECT_EQ(DeltaArchiveManifest_InstallOperation_Type_MOVE,
            graph.back().op.type());
  EXPECT_EQ(2, graph.back().op.src_extents_size());
  EXPECT_EQ(2, graph.back().op.dst_extents_size());
  EXPECT_EQ(0, graph.back().op.dst_extents(0).start_block());
  EXPECT_EQ(1, graph.back().op.dst_extents(0).num_blocks());
  EXPECT_EQ(8, graph.back().op.dst_extents(1).start_block());
  EXPECT_EQ(1, graph.back().op.dst_extents(1).num_blocks());
  EXPECT_TRUE(graph.back().out_edges.empty());
  
  // Check that old node reads from new blocks
  EXPECT_EQ(3, graph[0].op.src_extents_size());
  EXPECT_EQ(0, graph[0].op.src_extents(0).start_block());
  EXPECT_EQ(1, graph[0].op.src_extents(0).num_blocks());
  EXPECT_EQ(8, graph[0].op.src_extents(1).start_block());
  EXPECT_EQ(1, graph[0].op.src_extents(1).num_blocks());
  EXPECT_EQ(7, graph[0].op.src_extents(2).start_block());
  EXPECT_EQ(1, graph[0].op.src_extents(2).num_blocks());

  // And that the old dst extents haven't changed
  EXPECT_EQ(2, graph[0].op.dst_extents_size());
  EXPECT_EQ(1, graph[0].op.dst_extents(0).start_block());
  EXPECT_EQ(2, graph[0].op.dst_extents(0).num_blocks());
  EXPECT_EQ(4, graph[0].op.dst_extents(1).start_block());
  EXPECT_EQ(1, graph[0].op.dst_extents(1).num_blocks());

  // Ensure it only depends on the next node and the new temp node
  EXPECT_EQ(2, graph[0].out_edges.size());
  EXPECT_TRUE(graph[0].out_edges.end() != graph[0].out_edges.find(1));
  EXPECT_TRUE(graph[0].out_edges.end() != graph[0].out_edges.find(graph.size() -
                                                                  1));

  // Check second node has unchanged extents
  EXPECT_EQ(2, graph[1].op.src_extents_size());
  EXPECT_EQ(1, graph[1].op.src_extents(0).start_block());
  EXPECT_EQ(2, graph[1].op.src_extents(0).num_blocks());
  EXPECT_EQ(4, graph[1].op.src_extents(1).start_block());
  EXPECT_EQ(1, graph[1].op.src_extents(1).num_blocks());

  EXPECT_EQ(2, graph[1].op.dst_extents_size());
  EXPECT_EQ(3, graph[1].op.dst_extents(0).start_block());
  EXPECT_EQ(1, graph[1].op.dst_extents(0).num_blocks());
  EXPECT_EQ(5, graph[1].op.dst_extents(1).start_block());
  EXPECT_EQ(2, graph[1].op.dst_extents(1).num_blocks());
  
  // Ensure it only depends on the next node
  EXPECT_EQ(1, graph[1].out_edges.size());
  EXPECT_TRUE(graph[1].out_edges.end() != graph[1].out_edges.find(2));
}

TEST_F(DeltaDiffGeneratorTest, ReorderBlobsTest) {
  string orig_blobs;
  EXPECT_TRUE(
      utils::MakeTempFile("ReorderBlobsTest.orig.XXXXXX", &orig_blobs, NULL));

  string orig_data = "abcd";
  EXPECT_TRUE(
      utils::WriteFile(orig_blobs.c_str(), orig_data.data(), orig_data.size()));

  string new_blobs;
  EXPECT_TRUE(
      utils::MakeTempFile("ReorderBlobsTest.new.XXXXXX", &new_blobs, NULL));
  
  DeltaArchiveManifest manifest;
  DeltaArchiveManifest_InstallOperation* op =
      manifest.add_install_operations();
  op->set_data_offset(1);
  op->set_data_length(3);
  op = manifest.add_install_operations();
  op->set_data_offset(0);
  op->set_data_length(1);
  
  EXPECT_TRUE(DeltaDiffGenerator::ReorderDataBlobs(&manifest,
                                                   orig_blobs,
                                                   new_blobs));
                                                   
  string new_data;
  EXPECT_TRUE(utils::ReadFileToString(new_blobs, &new_data));
  EXPECT_EQ("bcda", new_data);
  EXPECT_EQ(2, manifest.install_operations_size());
  EXPECT_EQ(0, manifest.install_operations(0).data_offset());
  EXPECT_EQ(3, manifest.install_operations(0).data_length());
  EXPECT_EQ(3, manifest.install_operations(1).data_offset());
  EXPECT_EQ(1, manifest.install_operations(1).data_length());
  
  unlink(orig_blobs.c_str());
  unlink(new_blobs.c_str());
}

}  // namespace chromeos_update_engine
