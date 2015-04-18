// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "files/scoped_file.h"
#include "update_engine/cycle_breaker.h"
#include "update_engine/delta_diff_generator.h"
#include "update_engine/delta_performer.h"
#include "update_engine/extent_ranges.h"
#include "update_engine/graph_types.h"
#include "update_engine/graph_utils.h"
#include "update_engine/subprocess.h"
#include "update_engine/test_utils.h"
#include "update_engine/topological_sort.h"
#include "update_engine/utils.h"

using std::make_pair;
using std::set;
using std::string;
using std::stringstream;
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
  InstallOperation op;
  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_path(),
                                                 new_path(),
                                                 true, // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 true));
  EXPECT_TRUE(data.empty());

  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(InstallOperation_Type_MOVE, op.type());
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
  InstallOperation op;
  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_path(),
                                                 new_path(),
                                                 true, // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 true));
  EXPECT_FALSE(data.empty());

  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(InstallOperation_Type_BSDIFF, op.type());
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

TEST_F(DeltaDiffGeneratorTest, RunAsRootBsdiffNotAllowedTest) {
  EXPECT_TRUE(utils::WriteFile(old_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString) - 1));
  EXPECT_TRUE(utils::WriteFile(new_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString)));
  vector<char> data;
  InstallOperation op;

  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_path(),
                                                 new_path(),
                                                 false, // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 true));
  EXPECT_FALSE(data.empty());

  // The point of this test is that we don't use BSDIFF the way the above
  // did. The rest of the details are to be caught in other tests.
  EXPECT_TRUE(op.has_type());
  EXPECT_NE(InstallOperation_Type_BSDIFF, op.type());
}

TEST_F(DeltaDiffGeneratorTest, RunAsRootBsdiffNotAllowedMoveTest) {
  EXPECT_TRUE(utils::WriteFile(old_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString)));
  EXPECT_TRUE(utils::WriteFile(new_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString)));
  vector<char> data;
  InstallOperation op;

  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_path(),
                                                 new_path(),
                                                 false, // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 true));
  EXPECT_TRUE(data.empty());

  // The point of this test is that we can still use a MOVE for a file
  // that is blacklisted.
  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(InstallOperation_Type_MOVE, op.type());
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
    InstallOperation op;
    EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_path(),
                                                   new_path(),
                                                   true, // bsdiff_allowed
                                                   &data,
                                                   &op,
                                                   true));
    EXPECT_FALSE(data.empty());

    EXPECT_TRUE(op.has_type());
    const InstallOperation_Type expected_type =
        (i == 0 ? InstallOperation_Type_REPLACE :
         InstallOperation_Type_REPLACE_BZ);
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

TEST_F(DeltaDiffGeneratorTest, RunAsRootBsdiffNoGatherExtentsSmallTest) {
  EXPECT_TRUE(utils::WriteFile(old_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString) - 1));
  EXPECT_TRUE(utils::WriteFile(new_path().c_str(),
                               reinterpret_cast<const char*>(kRandomString),
                               sizeof(kRandomString)));
  vector<char> data;
  InstallOperation op;
  EXPECT_TRUE(DeltaDiffGenerator::ReadFileToDiff(old_path(),
                                                 new_path(),
                                                 true, // bsdiff_allowed
                                                 &data,
                                                 &op,
                                                 false));
  EXPECT_FALSE(data.empty());

  EXPECT_TRUE(op.has_type());
  EXPECT_EQ(InstallOperation_Type_BSDIFF, op.type());
  EXPECT_FALSE(op.has_data_offset());
  EXPECT_FALSE(op.has_data_length());
  EXPECT_EQ(1, op.src_extents_size());
  EXPECT_EQ(0, op.src_extents().Get(0).start_block());
  EXPECT_EQ(1, op.src_extents().Get(0).num_blocks());
  EXPECT_EQ(sizeof(kRandomString) - 1, op.src_length());
  EXPECT_EQ(1, op.dst_extents_size());
  EXPECT_EQ(0, op.dst_extents().Get(0).start_block());
  EXPECT_EQ(1, op.dst_extents().Get(0).num_blocks());
  EXPECT_EQ(sizeof(kRandomString), op.dst_length());
}

namespace {
void AppendExtent(vector<Extent>* vect, uint64_t start, uint64_t length) {
  vect->resize(vect->size() + 1);
  vect->back().set_start_block(start);
  vect->back().set_num_blocks(length);
}
void OpAppendExtent(InstallOperation* op,
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
  Vertex vertex;
  InstallOperation& op = vertex.op;
  OpAppendExtent(&op, 4, 3);
  OpAppendExtent(&op, kSparseHole, 4);  // Sparse hole in file
  OpAppendExtent(&op, 3, 1);
  OpAppendExtent(&op, 7, 3);

  DeltaDiffGenerator::SubstituteBlocks(&vertex, remove_blocks, replace_blocks);

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
    graph.back().op.set_type(InstallOperation_Type_MOVE);
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
    graph.back().op.set_type(InstallOperation_Type_MOVE);
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

  vector<CutEdgeVertexes> cuts;
  EXPECT_TRUE(DeltaDiffGenerator::CutEdges(&graph, cut_edges, &cuts));

  EXPECT_EQ(3, graph.size());

  // Check new node in graph:
  EXPECT_EQ(InstallOperation_Type_MOVE,
            graph.back().op.type());
  EXPECT_EQ(2, graph.back().op.src_extents_size());
  EXPECT_EQ(1, graph.back().op.dst_extents_size());
  EXPECT_EQ(kTempBlockStart, graph.back().op.dst_extents(0).start_block());
  EXPECT_EQ(2, graph.back().op.dst_extents(0).num_blocks());
  EXPECT_TRUE(graph.back().out_edges.empty());

  // Check that old node reads from new blocks
  EXPECT_EQ(2, graph[0].op.src_extents_size());
  EXPECT_EQ(kTempBlockStart, graph[0].op.src_extents(0).start_block());
  EXPECT_EQ(2, graph[0].op.src_extents(0).num_blocks());
  EXPECT_EQ(7, graph[0].op.src_extents(1).start_block());
  EXPECT_EQ(1, graph[0].op.src_extents(1).num_blocks());

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
  InstallOperation* op =
      manifest.add_partition_operations();
  op->set_data_offset(1);
  op->set_data_length(3);
  op = manifest.add_partition_operations();
  op->set_data_offset(0);
  op->set_data_length(1);

  EXPECT_TRUE(DeltaDiffGenerator::ReorderDataBlobs(&manifest,
                                                   orig_blobs,
                                                   new_blobs));

  string new_data;
  EXPECT_TRUE(utils::ReadFile(new_blobs, &new_data));
  EXPECT_EQ("bcda", new_data);
  EXPECT_EQ(2, manifest.partition_operations_size());
  EXPECT_EQ(0, manifest.partition_operations(0).data_offset());
  EXPECT_EQ(3, manifest.partition_operations(0).data_length());
  EXPECT_EQ(3, manifest.partition_operations(1).data_offset());
  EXPECT_EQ(1, manifest.partition_operations(1).data_length());

  unlink(orig_blobs.c_str());
  unlink(new_blobs.c_str());
}

TEST_F(DeltaDiffGeneratorTest, MoveFullOpsToBackTest) {
  Graph graph(4);
  graph[0].file_name = "A";
  graph[0].op.set_type(InstallOperation_Type_REPLACE);
  graph[1].file_name = "B";
  graph[1].op.set_type(InstallOperation_Type_BSDIFF);
  graph[2].file_name = "C";
  graph[2].op.set_type(InstallOperation_Type_REPLACE_BZ);
  graph[3].file_name = "D";
  graph[3].op.set_type(InstallOperation_Type_MOVE);

  vector<Vertex::Index> vect(graph.size());

  for (vector<Vertex::Index>::size_type i = 0; i < vect.size(); ++i) {
    vect[i] = i;
  }
  DeltaDiffGenerator::MoveFullOpsToBack(&graph, &vect);
  EXPECT_EQ(vect.size(), graph.size());
  EXPECT_EQ(graph[vect[0]].file_name, "B");
  EXPECT_EQ(graph[vect[1]].file_name, "D");
  EXPECT_EQ(graph[vect[2]].file_name, "A");
  EXPECT_EQ(graph[vect[3]].file_name, "C");
}

namespace {

#define OP_BSDIFF InstallOperation_Type_BSDIFF
#define OP_MOVE InstallOperation_Type_MOVE
#define OP_REPLACE InstallOperation_Type_REPLACE
#define OP_REPLACE_BZ InstallOperation_Type_REPLACE_BZ

void GenVertex(Vertex* out,
               const vector<Extent>& src_extents,
               const vector<Extent>& dst_extents,
               const string& path,
               InstallOperation_Type type) {
  out->op.set_type(type);
  out->file_name = path;
  DeltaDiffGenerator::StoreExtents(src_extents, out->op.mutable_src_extents());
  DeltaDiffGenerator::StoreExtents(dst_extents, out->op.mutable_dst_extents());
}

vector<Extent> VectOfExt(uint64_t start_block, uint64_t num_blocks) {
  return vector<Extent>(1, ExtentForRange(start_block, num_blocks));
}

EdgeProperties EdgeWithReadDep(const vector<Extent>& extents) {
  EdgeProperties ret;
  ret.extents = extents;
  return ret;
}

EdgeProperties EdgeWithWriteDep(const vector<Extent>& extents) {
  EdgeProperties ret;
  ret.write_extents = extents;
  return ret;
}

template<typename T>
void DumpVect(const vector<T>& vect) {
  std::stringstream ss(stringstream::out);
  for (typename vector<T>::const_iterator it = vect.begin(), e = vect.end();
       it != e; ++it) {
    ss << *it << ", ";
  }
  LOG(INFO) << "{" << ss.str() << "}";
}

}  // namespace {}

TEST_F(DeltaDiffGeneratorTest, RunAsRootAssignTempBlocksTest) {
  Graph graph(9);
  const vector<Extent> empt;  // empty
  const string kFilename = "/foo";

  // Some scratch space:
  GenVertex(&graph[0], empt, VectOfExt(200, 1), "", OP_REPLACE);
  GenVertex(&graph[1], empt, VectOfExt(210, 10), "", OP_REPLACE);
  GenVertex(&graph[2], empt, VectOfExt(220, 1), "", OP_REPLACE);

  // A cycle that requires 10 blocks to break:
  GenVertex(&graph[3], VectOfExt(10, 11), VectOfExt(0, 9), "", OP_BSDIFF);
  graph[3].out_edges[4] = EdgeWithReadDep(VectOfExt(0, 9));
  GenVertex(&graph[4], VectOfExt(0, 9), VectOfExt(10, 11), "", OP_BSDIFF);
  graph[4].out_edges[3] = EdgeWithReadDep(VectOfExt(10, 11));

  // A cycle that requires 9 blocks to break:
  GenVertex(&graph[5], VectOfExt(40, 11), VectOfExt(30, 10), "", OP_BSDIFF);
  graph[5].out_edges[6] = EdgeWithReadDep(VectOfExt(30, 10));
  GenVertex(&graph[6], VectOfExt(30, 10), VectOfExt(40, 11), "", OP_BSDIFF);
  graph[6].out_edges[5] = EdgeWithReadDep(VectOfExt(40, 11));

  // A cycle that requires 40 blocks to break (which is too many):
  GenVertex(&graph[7],
            VectOfExt(120, 50),
            VectOfExt(60, 40),
            "",
            OP_BSDIFF);
  graph[7].out_edges[8] = EdgeWithReadDep(VectOfExt(60, 40));
  GenVertex(&graph[8],
            VectOfExt(60, 40),
            VectOfExt(120, 50),
            kFilename,
            OP_BSDIFF);
  graph[8].out_edges[7] = EdgeWithReadDep(VectOfExt(120, 50));

  graph_utils::DumpGraph(graph);

  vector<Vertex::Index> final_order;


  // Prepare the filesystem with the minimum required for this to work
  string temp_dir;
  EXPECT_TRUE(utils::MakeTempDirectory("/tmp/AssignTempBlocksTest.XXXXXX",
                                       &temp_dir));
  ScopedDirRemover temp_dir_remover(temp_dir);

  const size_t kBlockSize = 4096;
  vector<char> temp_data(kBlockSize * 50);
  FillWithData(&temp_data);
  EXPECT_TRUE(WriteFileVector(temp_dir + kFilename, temp_data));
  ScopedPathUnlinker filename_unlinker(temp_dir + kFilename);

  int fd;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/AssignTempBlocksTestData.XXXXXX",
                                  NULL,
                                  &fd));
  files::ScopedFD fd_closer(fd);
  off_t data_file_size = 0;


  EXPECT_TRUE(DeltaDiffGenerator::ConvertGraphToDag(&graph,
                                                    temp_dir,
                                                    fd,
                                                    &data_file_size,
                                                    &final_order,
                                                    Vertex::kInvalidIndex));


  Graph expected_graph(12);
  GenVertex(&expected_graph[0], empt, VectOfExt(200, 1), "", OP_REPLACE);
  GenVertex(&expected_graph[1], empt, VectOfExt(210, 10), "", OP_REPLACE);
  GenVertex(&expected_graph[2], empt, VectOfExt(220, 1), "", OP_REPLACE);
  GenVertex(&expected_graph[3],
            VectOfExt(10, 11),
            VectOfExt(0, 9),
            "",
            OP_BSDIFF);
  expected_graph[3].out_edges[9] = EdgeWithReadDep(VectOfExt(0, 9));
  GenVertex(&expected_graph[4],
            VectOfExt(60, 9),
            VectOfExt(10, 11),
            "",
            OP_BSDIFF);
  expected_graph[4].out_edges[3] = EdgeWithReadDep(VectOfExt(10, 11));
  expected_graph[4].out_edges[9] = EdgeWithWriteDep(VectOfExt(60, 9));
  GenVertex(&expected_graph[5],
            VectOfExt(40, 11),
            VectOfExt(30, 10),
            "",
            OP_BSDIFF);
  expected_graph[5].out_edges[10] = EdgeWithReadDep(VectOfExt(30, 10));

  GenVertex(&expected_graph[6],
            VectOfExt(60, 10),
            VectOfExt(40, 11),
            "",
            OP_BSDIFF);
  expected_graph[6].out_edges[5] = EdgeWithReadDep(VectOfExt(40, 11));
  expected_graph[6].out_edges[10] = EdgeWithWriteDep(VectOfExt(60, 10));

  GenVertex(&expected_graph[7],
            VectOfExt(120, 50),
            VectOfExt(60, 40),
            "",
            OP_BSDIFF);
  expected_graph[7].out_edges[6] = EdgeWithReadDep(VectOfExt(60, 10));

  GenVertex(&expected_graph[8], empt, VectOfExt(0, 50), "/foo", OP_REPLACE_BZ);
  expected_graph[8].out_edges[7] = EdgeWithReadDep(VectOfExt(120, 50));

  GenVertex(&expected_graph[9],
            VectOfExt(0, 9),
            VectOfExt(60, 9),
            "",
            OP_MOVE);

  GenVertex(&expected_graph[10],
            VectOfExt(30, 10),
            VectOfExt(60, 10),
            "",
            OP_MOVE);
  expected_graph[10].out_edges[4] = EdgeWithReadDep(VectOfExt(60, 9));

  EXPECT_EQ(12, graph.size());
  EXPECT_FALSE(graph.back().valid);
  for (Graph::size_type i = 0; i < graph.size() - 1; i++) {
    EXPECT_TRUE(graph[i].out_edges == expected_graph[i].out_edges);
    if (i == 8) {
      // special case
    } else {
      // EXPECT_TRUE(graph[i] == expected_graph[i]) << "i = " << i;
    }
  }
}

// Test that sparse holes are not used as scratch. More specifically, test that
// if all full operations write to sparse holes and there's no extra scratch
// space, delta operations that need scratch are converted to full. See
// crbug.com/238440.
TEST_F(DeltaDiffGeneratorTest, RunAsRootNoSparseAsTempTest) {
  Graph graph(3);
  const vector<Extent> kEmpty;
  const string kFilename = "/foo";

  // Make sure this sparse hole is not used as scratch.
  GenVertex(&graph[0], kEmpty, VectOfExt(kSparseHole, 1), "", OP_REPLACE);

  // Create a single-block cycle.
  GenVertex(&graph[1], VectOfExt(0, 1), VectOfExt(1, 1), "", OP_BSDIFF);
  graph[1].out_edges[2] = EdgeWithReadDep(VectOfExt(1, 1));
  GenVertex(&graph[2], VectOfExt(1, 1), VectOfExt(0, 1), kFilename, OP_BSDIFF);
  graph[2].out_edges[1] = EdgeWithReadDep(VectOfExt(0, 1));

  graph_utils::DumpGraph(graph);

  vector<Vertex::Index> final_order;

  // Prepare the filesystem with the minimum required for this to work.
  string temp_dir;
  EXPECT_TRUE(utils::MakeTempDirectory("/tmp/NoSparseAsTempTest.XXXXXX",
                                       &temp_dir));
  ScopedDirRemover temp_dir_remover(temp_dir);

  const size_t kBlockSize = 4096;
  vector<char> temp_data(kBlockSize);
  FillWithData(&temp_data);
  EXPECT_TRUE(WriteFileVector(temp_dir + kFilename, temp_data));
  ScopedPathUnlinker filename_unlinker(temp_dir + kFilename);

  int fd = -1;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/NoSparseAsTempTestData.XXXXXX",
                                  NULL,
                                  &fd));
  files::ScopedFD fd_closer(fd);
  off_t data_file_size = 0;

  EXPECT_TRUE(DeltaDiffGenerator::ConvertGraphToDag(&graph,
                                                    temp_dir,
                                                    fd,
                                                    &data_file_size,
                                                    &final_order,
                                                    Vertex::kInvalidIndex));

  ASSERT_EQ(4, graph.size());

  // The second BSDIFF operation must have been converted to a full operation
  // (due to insufficient scratch space).
  EXPECT_TRUE(graph[2].op.type() == OP_REPLACE ||
              graph[2].op.type() == OP_REPLACE_BZ);

  // The temporary node created for breaking the cycle must have been marked as
  // invalid.
  EXPECT_FALSE(graph[3].valid);
}

// Test that sparse holes are not used as scratch. More specifically, test that
// if scratch space comes only from full operations writing to real blocks as
// well as sparse holes, only the real blocks are utilized. See
// crbug.com/238440.
TEST_F(DeltaDiffGeneratorTest, NoSparseAsTempTest) {
  Graph graph;
  vector<Block> blocks(4);

  // Create nodes in |graph|.
  {
    graph.resize(graph.size() + 1);
    graph.back().op.set_type(InstallOperation_Type_REPLACE);

    // Write to a sparse hole -- basically a no-op to ensure sparse holes are
    // not used as scratch.
    vector<Extent> extents;
    graph_utils::AppendBlockToExtents(&extents, kSparseHole);
    DeltaDiffGenerator::StoreExtents(extents,
                                     graph.back().op.mutable_dst_extents());
  }
  {
    graph.resize(graph.size() + 1);
    graph.back().op.set_type(OP_REPLACE);

    // Scratch space: write to block 0 with sparse holes around.
    vector<Extent> extents;
    graph_utils::AppendBlockToExtents(&extents, kSparseHole);
    graph_utils::AppendBlockToExtents(&extents, 0);
    graph_utils::AppendBlockToExtents(&extents, kSparseHole);
    DeltaDiffGenerator::StoreExtents(extents,
                                     graph.back().op.mutable_dst_extents());
    blocks[0].writer = graph.size() - 1;
  }
  {
    graph.resize(graph.size() + 1);
    graph.back().op.set_type(OP_REPLACE);

    // Write to a sparse hole.
    vector<Extent> extents;
    graph_utils::AppendBlockToExtents(&extents, kSparseHole);
    DeltaDiffGenerator::StoreExtents(extents,
                                     graph.back().op.mutable_dst_extents());
  }
  // Create a two-node cycle between (2, sparse, sparse) and (1, sparse, 3).
  {
    graph.resize(graph.size() + 1);
    graph.back().op.set_type(OP_MOVE);
    // Read from (2, sparse, sparse).
    vector<Extent> extents;
    graph_utils::AppendBlockToExtents(&extents, 2);
    graph_utils::AppendBlockToExtents(&extents, kSparseHole);
    graph_utils::AppendBlockToExtents(&extents, kSparseHole);
    DeltaDiffGenerator::StoreExtents(extents,
                                     graph.back().op.mutable_src_extents());
    blocks[2].reader = graph.size() - 1;

    // Write to (1, sparse, 3).
    extents.clear();
    graph_utils::AppendBlockToExtents(&extents, 1);
    graph_utils::AppendBlockToExtents(&extents, kSparseHole);
    graph_utils::AppendBlockToExtents(&extents, 3);
    DeltaDiffGenerator::StoreExtents(extents,
                                     graph.back().op.mutable_dst_extents());
    blocks[1].writer = graph.size() - 1;
    blocks[3].writer = graph.size() - 1;
  }
  {
    graph.resize(graph.size() + 1);
    graph.back().op.set_type(OP_MOVE);
    // Read from (1, sparse, 3).
    vector<Extent> extents;
    graph_utils::AppendBlockToExtents(&extents, 1);
    graph_utils::AppendBlockToExtents(&extents, kSparseHole);
    graph_utils::AppendBlockToExtents(&extents, 3);
    DeltaDiffGenerator::StoreExtents(extents,
                                     graph.back().op.mutable_src_extents());
    blocks[1].reader = graph.size() - 1;
    blocks[3].reader = graph.size() - 1;

    // Write to (2, sparse, sparse).
    extents.clear();
    graph_utils::AppendBlockToExtents(&extents, 2);
    graph_utils::AppendBlockToExtents(&extents, kSparseHole);
    graph_utils::AppendBlockToExtents(&extents, kSparseHole);
    DeltaDiffGenerator::StoreExtents(extents,
                                     graph.back().op.mutable_dst_extents());
    blocks[2].writer = graph.size() - 1;
  }

  graph_utils::DumpGraph(graph);

  // Create edges
  DeltaDiffGenerator::CreateEdges(&graph, blocks);

  graph_utils::DumpGraph(graph);

  vector<Vertex::Index> final_order;
  off_t data_file_size = 0;
  EXPECT_TRUE(DeltaDiffGenerator::ConvertGraphToDag(&graph,
                                                    "/non/existent/dir",
                                                    -1,
                                                    &data_file_size,
                                                    &final_order,
                                                    Vertex::kInvalidIndex));

  // Check for a single temporary node writing to scratch.
  ASSERT_EQ(6, graph.size());
  EXPECT_EQ(InstallOperation_Type_MOVE, graph[5].op.type());
  EXPECT_EQ(1, graph[5].op.src_extents_size());
  ASSERT_EQ(1, graph[5].op.dst_extents_size());
  EXPECT_EQ(0, graph[5].op.dst_extents(0).start_block());
  EXPECT_EQ(1, graph[5].op.dst_extents(0).num_blocks());

  // Make sure the cycle nodes still read from and write to sparse holes.
  for (int i = 3; i < 5; i++) {
    ASSERT_GE(graph[i].op.src_extents_size(), 2);
    EXPECT_EQ(kSparseHole, graph[i].op.src_extents(1).start_block());
    ASSERT_GE(graph[i].op.dst_extents_size(), 2);
    EXPECT_EQ(kSparseHole, graph[i].op.dst_extents(1).start_block());
  }
}

TEST_F(DeltaDiffGeneratorTest, IsNoopOperationTest) {
  InstallOperation op;
  op.set_type(InstallOperation_Type_REPLACE_BZ);
  EXPECT_FALSE(DeltaDiffGenerator::IsNoopOperation(op));
  op.set_type(InstallOperation_Type_MOVE);
  EXPECT_TRUE(DeltaDiffGenerator::IsNoopOperation(op));
  *(op.add_src_extents()) = ExtentForRange(3, 2);
  *(op.add_dst_extents()) = ExtentForRange(3, 2);
  EXPECT_TRUE(DeltaDiffGenerator::IsNoopOperation(op));
  *(op.add_src_extents()) = ExtentForRange(7, 5);
  *(op.add_dst_extents()) = ExtentForRange(7, 5);
  EXPECT_TRUE(DeltaDiffGenerator::IsNoopOperation(op));
  *(op.add_src_extents()) = ExtentForRange(20, 2);
  *(op.add_dst_extents()) = ExtentForRange(20, 1);
  *(op.add_dst_extents()) = ExtentForRange(21, 1);
  EXPECT_TRUE(DeltaDiffGenerator::IsNoopOperation(op));
  *(op.add_src_extents()) = ExtentForRange(kSparseHole, 2);
  *(op.add_src_extents()) = ExtentForRange(kSparseHole, 1);
  *(op.add_dst_extents()) = ExtentForRange(kSparseHole, 3);
  EXPECT_TRUE(DeltaDiffGenerator::IsNoopOperation(op));
  *(op.add_src_extents()) = ExtentForRange(24, 1);
  *(op.add_dst_extents()) = ExtentForRange(25, 1);
  EXPECT_FALSE(DeltaDiffGenerator::IsNoopOperation(op));
}

TEST_F(DeltaDiffGeneratorTest, RunAsRootAssignTempBlocksReuseTest) {
  // AssignTempBlocks(Graph* graph,
  // const string& new_root,
  // int data_fd,
  // off_t* data_file_size,
  // vector<Vertex::Index>* op_indexes,
  // vector<vector<Vertex::Index>::size_type>* reverse_op_indexes,
  // const vector<CutEdgeVertexes>& cuts
  Graph graph(9);

  const vector<Extent> empt;
  uint64_t tmp = kTempBlockStart;
  const string kFilename = "/foo";

  vector<CutEdgeVertexes> cuts;
  cuts.resize(3);

  // Simple broken loop:
  GenVertex(&graph[0], VectOfExt(0, 1), VectOfExt(1, 1), "", OP_MOVE);
  GenVertex(&graph[1], VectOfExt(tmp, 1), VectOfExt(0, 1), "", OP_MOVE);
  GenVertex(&graph[2], VectOfExt(1, 1), VectOfExt(tmp, 1), "", OP_MOVE);
  // Corresponding edges:
  graph[0].out_edges[2] = EdgeWithReadDep(VectOfExt(1, 1));
  graph[1].out_edges[2] = EdgeWithWriteDep(VectOfExt(tmp, 1));
  graph[1].out_edges[0] = EdgeWithReadDep(VectOfExt(0, 1));
  // Store the cut:
  cuts[0].old_dst = 1;
  cuts[0].old_src = 0;
  cuts[0].new_vertex = 2;
  cuts[0].tmp_extents = VectOfExt(tmp, 1);
  tmp++;

  // Slightly more complex pair of loops:
  GenVertex(&graph[3], VectOfExt(4, 2), VectOfExt(2, 2), "", OP_MOVE);
  GenVertex(&graph[4], VectOfExt(6, 1), VectOfExt(7, 1), "", OP_MOVE);
  GenVertex(&graph[5], VectOfExt(tmp, 3), VectOfExt(4, 3), kFilename, OP_MOVE);
  GenVertex(&graph[6], VectOfExt(2, 2), VectOfExt(tmp, 2), "", OP_MOVE);
  GenVertex(&graph[7], VectOfExt(7, 1), VectOfExt(tmp + 2, 1), "", OP_MOVE);
  // Corresponding edges:
  graph[3].out_edges[6] = EdgeWithReadDep(VectOfExt(2, 2));
  graph[4].out_edges[7] = EdgeWithReadDep(VectOfExt(7, 1));
  graph[5].out_edges[6] = EdgeWithWriteDep(VectOfExt(tmp, 2));
  graph[5].out_edges[7] = EdgeWithWriteDep(VectOfExt(tmp + 2, 1));
  graph[5].out_edges[3] = EdgeWithReadDep(VectOfExt(4, 2));
  graph[5].out_edges[4] = EdgeWithReadDep(VectOfExt(6, 1));
  // Store the cuts:
  cuts[1].old_dst = 5;
  cuts[1].old_src = 3;
  cuts[1].new_vertex = 6;
  cuts[1].tmp_extents = VectOfExt(tmp, 2);
  cuts[2].old_dst = 5;
  cuts[2].old_src = 4;
  cuts[2].new_vertex = 7;
  cuts[2].tmp_extents = VectOfExt(tmp + 2, 1);

  // Supplier of temp block:
  GenVertex(&graph[8], empt, VectOfExt(8, 1), "", OP_REPLACE);

  // Specify the final order:
  vector<Vertex::Index> op_indexes;
  op_indexes.push_back(2);
  op_indexes.push_back(0);
  op_indexes.push_back(1);
  op_indexes.push_back(6);
  op_indexes.push_back(3);
  op_indexes.push_back(7);
  op_indexes.push_back(4);
  op_indexes.push_back(5);
  op_indexes.push_back(8);

  vector<vector<Vertex::Index>::size_type> reverse_op_indexes;
  DeltaDiffGenerator::GenerateReverseTopoOrderMap(op_indexes,
                                                  &reverse_op_indexes);

  // Prepare the filesystem with the minimum required for this to work
  string temp_dir;
  EXPECT_TRUE(utils::MakeTempDirectory("/tmp/AssignTempBlocksReuseTest.XXXXXX",
                                       &temp_dir));
  ScopedDirRemover temp_dir_remover(temp_dir);

  const size_t kBlockSize = 4096;
  vector<char> temp_data(kBlockSize * 3);
  FillWithData(&temp_data);
  EXPECT_TRUE(WriteFileVector(temp_dir + kFilename, temp_data));
  ScopedPathUnlinker filename_unlinker(temp_dir + kFilename);

  int fd;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/AssignTempBlocksReuseTest.XXXXXX",
                                  NULL,
                                  &fd));
  files::ScopedFD fd_closer(fd);
  off_t data_file_size = 0;

  EXPECT_TRUE(DeltaDiffGenerator::AssignTempBlocks(&graph,
                                                   temp_dir,
                                                   fd,
                                                   &data_file_size,
                                                   &op_indexes,
                                                   &reverse_op_indexes,
                                                   cuts));
  EXPECT_FALSE(graph[6].valid);
  EXPECT_FALSE(graph[7].valid);
  EXPECT_EQ(1, graph[1].op.src_extents_size());
  EXPECT_EQ(2, graph[1].op.src_extents(0).start_block());
  EXPECT_EQ(1, graph[1].op.src_extents(0).num_blocks());
  EXPECT_EQ(OP_REPLACE_BZ, graph[5].op.type());
}

TEST_F(DeltaDiffGeneratorTest, CreateScratchNodeTest) {
  Vertex vertex;
  DeltaDiffGenerator::CreateScratchNode(12, 34, &vertex);
  EXPECT_EQ(InstallOperation_Type_REPLACE_BZ,
            vertex.op.type());
  EXPECT_EQ(0, vertex.op.data_offset());
  EXPECT_EQ(0, vertex.op.data_length());
  EXPECT_EQ(1, vertex.op.dst_extents_size());
  EXPECT_EQ(12, vertex.op.dst_extents(0).start_block());
  EXPECT_EQ(34, vertex.op.dst_extents(0).num_blocks());
}

}  // namespace chromeos_update_engine
