// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "files/scoped_file.h"
#include "update_engine/full_update_generator.h"
#include "update_engine/test_utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
  const size_t kBlockSize = 4096;
}  // namespace {}


class FullUpdateGeneratorTest : public ::testing::Test { };


TEST(FullUpdateGeneratorTest, RunTest) {
  vector<char> new_root(20 * 1024 * 1024);
  vector<char> new_kern(16 * 1024 * 1024);
  const off_t kChunkSize = 128 * 1024;
  FillWithData(&new_root);
  FillWithData(&new_kern);
  // Assume hashes take 2 MiB beyond the rootfs.
  off_t new_rootfs_size = new_root.size() - 2 * 1024 * 1024;

  string new_root_path;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/NewFullUpdateTest_R.XXXXXX",
                                  &new_root_path,
                                  NULL));
  ScopedPathUnlinker new_root_path_unlinker(new_root_path);
  EXPECT_TRUE(WriteFileVector(new_root_path, new_root));

  string new_kern_path;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/NewFullUpdateTest_K.XXXXXX",
                                  &new_kern_path,
                                  NULL));
  ScopedPathUnlinker new_kern_path_unlinker(new_kern_path);
  EXPECT_TRUE(WriteFileVector(new_kern_path, new_kern));

  string out_blobs_path;
  int out_blobs_fd;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/NewFullUpdateTest_D.XXXXXX",
                                  &out_blobs_path,
                                  &out_blobs_fd));
  ScopedPathUnlinker out_blobs_path_unlinker(out_blobs_path);
  files::ScopedFD out_blobs_fd_closer(out_blobs_fd);

  Graph graph;
  vector<InstallOperation> kernel_ops;
  vector<Vertex::Index> final_order;
  FullUpdateGenerator generator(out_blobs_fd, kChunkSize, kBlockSize);

  EXPECT_TRUE(generator.Partition(new_root_path,
                                  new_rootfs_size,
                                  &graph,
                                  &final_order));
  EXPECT_TRUE(generator.Add(new_kern_path, &kernel_ops));

  EXPECT_EQ(new_rootfs_size / kChunkSize, graph.size());
  EXPECT_EQ(new_rootfs_size / kChunkSize, final_order.size());
  EXPECT_EQ(new_kern.size() / kChunkSize, kernel_ops.size());

  off_t out_offset = 0;
  for (off_t i = 0; i < (new_rootfs_size / kChunkSize); ++i) {
    EXPECT_EQ(i, final_order[i]);
    EXPECT_EQ(1, graph[i].op.dst_extents_size());
    EXPECT_EQ(i * kChunkSize / kBlockSize,
              graph[i].op.dst_extents(0).start_block()) << "i = " << i;
    EXPECT_EQ(kChunkSize / kBlockSize,
              graph[i].op.dst_extents(0).num_blocks());
    if (graph[i].op.type() !=
        InstallOperation_Type_REPLACE) {
      EXPECT_EQ(InstallOperation_Type_REPLACE_BZ,
                graph[i].op.type());
    }
    EXPECT_EQ(out_offset, graph[i].op.data_offset());
    out_offset += graph[i].op.data_length();
  }
  for (size_t i = 0; i < kernel_ops.size(); ++i) {
    EXPECT_EQ(1, kernel_ops[i].dst_extents_size());
    EXPECT_EQ(i * kChunkSize / kBlockSize,
              kernel_ops[i].dst_extents(0).start_block()) << "i = " << i;
    EXPECT_EQ(kChunkSize / kBlockSize,
              kernel_ops[i].dst_extents(0).num_blocks());
    if (kernel_ops[i].type() !=
        InstallOperation_Type_REPLACE) {
      EXPECT_EQ(InstallOperation_Type_REPLACE_BZ,
                kernel_ops[i].type());
    }
    EXPECT_EQ(out_offset, kernel_ops[i].data_offset());
    out_offset += kernel_ops[i].data_length();
  }
  EXPECT_EQ(out_offset, generator.Size());
  EXPECT_EQ(out_offset, utils::FileSize(out_blobs_path));
}

}  // namespace chromeos_update_engine
