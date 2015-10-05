// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "files/scoped_file.h"
#include "strings/string_printf.h"
#include "update_engine/delta_diff_generator.h"
#include "update_engine/ext2_metadata.h"
#include "update_engine/graph_types.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;
using strings::StringPrintf;

namespace chromeos_update_engine {

typedef DeltaDiffGenerator::Block Block;

class Ext2MetadataTest : public ::testing::Test {
};

TEST_F(Ext2MetadataTest, RunAsRootReadMetadataDissimilarFileSystems) {
  string a_img, b_img;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/a_img.XXXXXX", &a_img, NULL));
  ScopedPathUnlinker a_img_unlinker(a_img);
  EXPECT_TRUE(utils::MakeTempFile("/tmp/b_img.XXXXXX", &b_img, NULL));
  ScopedPathUnlinker b_img_unlinker(b_img);

  CreateEmptyExtImageAtPath(a_img, 10485759, 4096);
  CreateEmptyExtImageAtPath(b_img, 11534336, 4096);

  Graph graph;
  vector<Block> blocks;
  EXPECT_TRUE(Ext2Metadata::DeltaReadMetadata(&graph,
                                              &blocks,
                                              a_img,
                                              b_img,
                                              0,
                                              NULL));
  EXPECT_EQ(graph.size(), 0);

  CreateEmptyExtImageAtPath(a_img, 10485759, 4096);
  CreateEmptyExtImageAtPath(b_img, 10485759, 8192);

  graph.clear();
  blocks.clear();
  EXPECT_TRUE(Ext2Metadata::DeltaReadMetadata(&graph,
                                              &blocks,
                                              a_img,
                                              b_img,
                                              0,
                                              NULL));
  EXPECT_EQ(graph.size(), 0);
}

TEST_F(Ext2MetadataTest, RunAsRootReadMetadata) {
  string a_img, b_img, data_file;
  EXPECT_TRUE(utils::MakeTempFile("/tmp/a_img.XXXXXX", &a_img, NULL));
  ScopedPathUnlinker a_img_unlinker(a_img);
  EXPECT_TRUE(utils::MakeTempFile("/tmp/b_img.XXXXXX", &b_img, NULL));
  ScopedPathUnlinker b_img_unlinker(b_img);
  EXPECT_TRUE(utils::MakeTempFile("/tmp/data_file.XXXXXX", &data_file, NULL));
  ScopedPathUnlinker data_file_unlinker(data_file);

  const size_t image_size = (256 * 1024 * 1024);  // Enough for 2 block groups
  const int block_size = 4096;
  CreateEmptyExtImageAtPath(a_img, image_size, block_size);

  // Create a file large enough to create an indirect block
  {
    string a_img_mnt;
    ScopedLoopMounter a_img_mount(a_img, &a_img_mnt, 0);
    System(StringPrintf("dd if=/dev/zero of=%s/test_file bs=%d count=%d",
                        a_img_mnt.c_str(), block_size, EXT2_NDIR_BLOCKS + 1));
  }

  System(StringPrintf("cp %s %s", a_img.c_str(), b_img.c_str()));

  int fd = open(data_file.c_str(), O_RDWR | O_CREAT, S_IRWXU);
  EXPECT_NE(fd, -1);
  files::ScopedFD fd_closer(fd);

  Graph graph;
  vector<Block> blocks(image_size / block_size);
  off_t data_file_size;
  EXPECT_TRUE(Ext2Metadata::DeltaReadMetadata(&graph,
                                              &blocks,
                                              a_img,
                                              b_img,
                                              fd,
                                              &data_file_size));

  // There are 12 metadata that we look for:
  //   - Block group 0 metadata (superblock, group descriptor, bitmaps, etc)
  //       - Chunk 0, 1, 2, 3
  //   - Block group 1 metadata
  //       - Chunk 0, 1, 2, 3
  //   - Root directory (inode 2)
  //   - Journal (inode 8)
  //   - lost+found directory (inode 11)
  //   - test_file indirect block (inode 12)
  struct {
    string metadata_name;
    off_t start_block; // Set to -1 to skip start block verification
    off_t num_blocks; // Set to -1 to skip num blocks verification
  } exp_results[] =
      {{"<fs-bg-0-0-metadata>", 0, 104},
       {"<fs-bg-0-1-metadata>", 104, 104},
       {"<fs-bg-0-2-metadata>", 208, 104},
       {"<fs-bg-0-3-metadata>", 312, 104},
       {"<fs-bg-0-4-metadata>", 416, 104},
       {"<fs-bg-0-5-metadata>", 520, 104},
       {"<fs-bg-0-6-metadata>", 624, 104},
       {"<fs-bg-0-7-metadata>", 728, 104},
       {"<fs-bg-0-8-metadata>", 832, 104},
       {"<fs-bg-0-9-metadata>", 936, 107},
       {"<fs-bg-1-0-metadata>", 32768, 104},
       {"<fs-bg-1-1-metadata>", 32872, 104},
       {"<fs-bg-1-2-metadata>", 32976, 104},
       {"<fs-bg-1-3-metadata>", 33080, 104},
       {"<fs-bg-1-4-metadata>", 33184, 104},
       {"<fs-bg-1-5-metadata>", 33288, 104},
       {"<fs-bg-1-6-metadata>", 33392, 104},
       {"<fs-bg-1-7-metadata>", 33496, 104},
       {"<fs-bg-1-8-metadata>", 33600, 104},
       {"<fs-bg-1-9-metadata>", 33704, 107},
       {"<fs-inode-2-metadata>", -1, 1},
       {"<fs-inode-8-metadata>", -1, 4101},
       {"<fs-inode-11-metadata>", -1, 4},
       {"<fs-inode-12-metadata>", -1, 1}};

  int num_exp_results = sizeof(exp_results) / sizeof(exp_results[0]);
  EXPECT_EQ(graph.size(), num_exp_results);

  for (int i = 0; i < num_exp_results; i++) {
    Vertex& vertex = graph[i];
    InstallOperation& op = vertex.op;

    EXPECT_STRCASEEQ(vertex.file_name.c_str(),
                     exp_results[i].metadata_name.c_str());

    EXPECT_EQ(op.src_extents().size(), op.dst_extents().size());
    for (int e = 0; e < op.src_extents().size(); e++) {
      EXPECT_EQ(op.src_extents(e).start_block(),
                op.dst_extents(e).start_block());
      EXPECT_EQ(op.src_extents(e).num_blocks(),
                op.dst_extents(e).num_blocks());
    }

    if (exp_results[i].start_block != -1) {
      EXPECT_EQ(op.src_extents(0).start_block(), exp_results[i].start_block);
    }

    if (exp_results[i].num_blocks != -1) {
      EXPECT_EQ(op.src_extents(0).num_blocks(), exp_results[i].num_blocks);
    }
  }
}

}  // namespace chromeos_update_engine
