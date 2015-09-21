// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include <et/com_err.h>
#include <ext2fs/ext2_io.h>
#include <ext2fs/ext2fs.h>

#include "files/scoped_file.h"
#include "strings/string_printf.h"
#include "update_engine/bzip.h"
#include "update_engine/delta_diff_generator.h"
#include "update_engine/ext2_metadata.h"
#include "update_engine/extent_ranges.h"
#include "update_engine/graph_utils.h"
#include "update_engine/utils.h"

using std::min;
using std::string;
using std::vector;
using strings::StringPrintf;

namespace chromeos_update_engine {

namespace {
const size_t kBlockSize = 4096;

typedef DeltaDiffGenerator::Block Block;

// Utility class to close a file system
class ScopedExt2fsCloser {
 public:
  explicit ScopedExt2fsCloser(ext2_filsys filsys) : filsys_(filsys) {}
  ~ScopedExt2fsCloser() { ext2fs_close(filsys_); }

 private:
  ext2_filsys filsys_;
  DISALLOW_COPY_AND_ASSIGN(ScopedExt2fsCloser);
};

// Read data from the specified extents.
bool ReadExtentsData(const ext2_filsys fs,
                     const vector<Extent>& extents,
                     vector<char>* data) {
  // Resize the data buffer to hold all data in the extents
  size_t num_data_blocks = 0;
  for (const Extent& extent : extents) {
    num_data_blocks += extent.num_blocks();
  }

  data->resize(num_data_blocks * kBlockSize);

  // Read in the data blocks
  const size_t kMaxReadBlocks = 256;
  vector<Block>::size_type blocks_copied_count = 0;
  for (const Extent& extent : extents) {
    vector<Block>::size_type blocks_read = 0;
    while (blocks_read < extent.num_blocks()) {
      const int copy_block_cnt =
          min(kMaxReadBlocks,
              static_cast<size_t>(
                  extent.num_blocks() - blocks_read));
      TEST_AND_RETURN_FALSE_ERRCODE(
          io_channel_read_blk(fs->io,
                              extent.start_block() + blocks_read,
                              copy_block_cnt,
                              &(*data)[blocks_copied_count * kBlockSize]));
      blocks_read += copy_block_cnt;
      blocks_copied_count += copy_block_cnt;
    }
  }

  return true;
}

// Compute the bsdiff between two metadata blobs.
bool ComputeMetadataBsdiff(const vector<char>& old_metadata,
                           const vector<char>& new_metadata,
                           vector<char>* bsdiff_delta) {
  const string kTempFileTemplate("/tmp/CrAU_temp_data.XXXXXX");

  // Write the metadata buffers to temporary files
  int old_fd;
  string temp_old_file_path;
  TEST_AND_RETURN_FALSE(
      utils::MakeTempFile(kTempFileTemplate, &temp_old_file_path, &old_fd));
  TEST_AND_RETURN_FALSE(old_fd >= 0);
  ScopedPathUnlinker temp_old_file_path_unlinker(temp_old_file_path);
  files::ScopedFD old_fd_closer(old_fd);
  TEST_AND_RETURN_FALSE(utils::WriteAll(old_fd,
                                        &old_metadata[0],
                                        old_metadata.size()));

  int new_fd;
  string temp_new_file_path;
  TEST_AND_RETURN_FALSE(
      utils::MakeTempFile(kTempFileTemplate, &temp_new_file_path, &new_fd));
  TEST_AND_RETURN_FALSE(new_fd >= 0);
  ScopedPathUnlinker temp_new_file_path_unlinker(temp_new_file_path);
  files::ScopedFD new_fd_closer(new_fd);
  TEST_AND_RETURN_FALSE(utils::WriteAll(new_fd,
                                        &new_metadata[0],
                                        new_metadata.size()));

  // Perform bsdiff on these files
  TEST_AND_RETURN_FALSE(
      DeltaDiffGenerator::BsdiffFiles(temp_old_file_path,
                                      temp_new_file_path,
                                      bsdiff_delta));

  return true;
}

// Add the specified metadata extents to the graph and blocks vector.
bool AddMetadataExtents(Graph* graph,
                        vector<Block>* blocks,
                        const ext2_filsys fs_old,
                        const ext2_filsys fs_new,
                        const string& metadata_name,
                        const vector<Extent>& extents,
                        int data_fd,
                        off_t* data_file_size) {
  vector<char> data;  // Data blob that will be written to delta file.
  InstallOperation op;

  {
    // Read in the metadata blocks from the old and new image.
    vector<char> old_data;
    TEST_AND_RETURN_FALSE(ReadExtentsData(fs_old, extents, &old_data));

    vector<char> new_data;
    TEST_AND_RETURN_FALSE(ReadExtentsData(fs_new, extents, &new_data));

    // Determine the best way to compress this.
    vector<char> new_data_bz;
    TEST_AND_RETURN_FALSE(BzipCompress(new_data, &new_data_bz));
    CHECK(!new_data_bz.empty());

    size_t current_best_size = 0;
    if (new_data.size() <= new_data_bz.size()) {
      op.set_type(InstallOperation_Type_REPLACE);
      current_best_size = new_data.size();
      data = new_data;
    } else {
      op.set_type(InstallOperation_Type_REPLACE_BZ);
      current_best_size = new_data_bz.size();
      data = new_data_bz;
    }

    if (old_data == new_data) {
      // No change in data.
      op.set_type(InstallOperation_Type_MOVE);
      current_best_size = 0;
      data.clear();
    } else {
      // Try bsdiff of old to new data
      vector<char> bsdiff_delta;
      TEST_AND_RETURN_FALSE(ComputeMetadataBsdiff(old_data,
                                                  new_data,
                                                  &bsdiff_delta));
      CHECK_GT(bsdiff_delta.size(), static_cast<vector<char>::size_type>(0));

      if (bsdiff_delta.size() < current_best_size) {
        op.set_type(InstallOperation_Type_BSDIFF);
        current_best_size = bsdiff_delta.size();
        data = bsdiff_delta;
      }
    }

    CHECK_EQ(data.size(), current_best_size);

    // Set the source and dest extents to be the same since the filesystem
    // structures are identical
    if (op.type() == InstallOperation_Type_MOVE ||
        op.type() == InstallOperation_Type_BSDIFF) {
      DeltaDiffGenerator::StoreExtents(extents, op.mutable_src_extents());
      op.set_src_length(old_data.size());
    }

    DeltaDiffGenerator::StoreExtents(extents, op.mutable_dst_extents());
    op.set_dst_length(new_data.size());
  }

  // Write data to output file
  if (op.type() != InstallOperation_Type_MOVE) {
    op.set_data_offset(*data_file_size);
    op.set_data_length(data.size());
  }

  TEST_AND_RETURN_FALSE(utils::WriteAll(data_fd, &data[0], data.size()));
  *data_file_size += data.size();

  // Now, insert into graph and blocks vector
  graph->resize(graph->size() + 1);
  Vertex::Index vertex = graph->size() - 1;
  (*graph)[vertex].op = op;
  CHECK((*graph)[vertex].op.has_type());
  (*graph)[vertex].file_name = metadata_name;

  TEST_AND_RETURN_FALSE(DeltaDiffGenerator::AddInstallOpToBlocksVector(
      (*graph)[vertex].op,
      *graph,
      vertex,
      blocks));

  return true;
}

// Reads the file system metadata extents.
bool ReadFilesystemMetadata(Graph* graph,
                            vector<Block>* blocks,
                            const ext2_filsys fs_old,
                            const ext2_filsys fs_new,
                            int data_fd,
                            off_t* data_file_size) {
  LOG(INFO) << "Processing <fs-metadata>";

  // Read all the extents that belong to the main file system metadata.
  // The metadata blocks are at the start of each block group and goes
  // until the end of the inode table.
  for (dgrp_t bg = 0; bg < fs_old->group_desc_count; bg++) {
    struct ext2_group_desc* group_desc = ext2fs_group_desc(fs_old,
                                                           fs_old->group_desc,
                                                           bg);
    __u32 num_metadata_blocks = (group_desc->bg_inode_table +
                                 fs_old->inode_blocks_per_group) -
                                 (bg * fs_old->super->s_blocks_per_group);
    __u32 bg_start_block = bg * fs_old->super->s_blocks_per_group;

    // Due to bsdiff slowness, we're going to break each block group down
    // into metadata chunks and feed them to bsdiff.
    __u32 num_chunks = 10;
    __u32 blocks_per_chunk = num_metadata_blocks / num_chunks;
    __u32 curr_block = bg_start_block;
    for (__u32 chunk = 0; chunk < num_chunks; chunk++) {
      Extent extent;
      if (chunk < num_chunks - 1) {
        extent = ExtentForRange(curr_block, blocks_per_chunk);
      } else {
        extent = ExtentForRange(curr_block,
                                bg_start_block + num_metadata_blocks -
                                curr_block);
      }

      vector<Extent> extents;
      extents.push_back(extent);

      string metadata_name = StringPrintf("<fs-bg-%d-%d-metadata>",
                                          bg, chunk);

      LOG(INFO) << "Processing " << metadata_name;

      TEST_AND_RETURN_FALSE(AddMetadataExtents(graph,
                                               blocks,
                                               fs_old,
                                               fs_new,
                                               metadata_name,
                                               extents,
                                               data_fd,
                                               data_file_size));

      curr_block += blocks_per_chunk;
    }
  }

  return true;
}

// Processes all blocks belonging to an inode
int ProcessInodeAllBlocks(ext2_filsys fs,
                          blk_t* blocknr,
                          e2_blkcnt_t blockcnt,
                          blk_t ref_blk,
                          int ref_offset,
                          void* priv) {
  vector<Extent>* extents = static_cast<vector<Extent>*>(priv);
  graph_utils::AppendBlockToExtents(extents, *blocknr);
  return 0;
}

// Processes only indirect, double indirect or triple indirect metadata
// blocks belonging to an inode
int ProcessInodeMetadataBlocks(ext2_filsys fs,
                               blk_t* blocknr,
                               e2_blkcnt_t blockcnt,
                               blk_t ref_blk,
                               int ref_offset,
                               void* priv) {
  vector<Extent>* extents = static_cast<vector<Extent>*>(priv);
  if (blockcnt < 0) {
    graph_utils::AppendBlockToExtents(extents, *blocknr);
  }
  return 0;
}

// Read inode metadata blocks.
bool ReadInodeMetadata(Graph* graph,
                       vector<Block>* blocks,
                       const ext2_filsys fs_old,
                       const ext2_filsys fs_new,
                       int data_fd,
                       off_t* data_file_size) {
  TEST_AND_RETURN_FALSE_ERRCODE(ext2fs_read_inode_bitmap(fs_old));
  TEST_AND_RETURN_FALSE_ERRCODE(ext2fs_read_inode_bitmap(fs_new));

  ext2_inode_scan iscan;
  TEST_AND_RETURN_FALSE_ERRCODE(ext2fs_open_inode_scan(fs_old, 0, &iscan));

  ext2_ino_t ino;
  ext2_inode old_inode;
  while (true) {
    // Get the next inode on both file systems
    errcode_t error = ext2fs_get_next_inode(iscan, &ino, &old_inode);

    // If we get an error enumerating the inodes, we'll just log the error
    // and exit from our loop which will eventually return a success code
    // back to the caller. The inode blocks that we cannot account for will
    // be handled by DeltaDiffGenerator::ReadUnwrittenBlocks().
    if (error) {
      LOG(ERROR) << "Failed to retrieve next inode (" << error << ")";
      break;
    }

    if (ino == 0) {
      break;
    }

    if (ino == EXT2_RESIZE_INO) {
      continue;
    }

    ext2_inode new_inode;
    error = ext2fs_read_inode(fs_new, ino, &new_inode);
    if (error) {
      LOG(ERROR) << "Failed to retrieve new inode (" << error << ")";
      continue;
    }

    // Skip inodes that are not in use
    if (!ext2fs_test_inode_bitmap(fs_old->inode_map, ino)  ||
        !ext2fs_test_inode_bitmap(fs_new->inode_map, ino)) {
      continue;
    }

    // Skip inodes that have no data blocks
    if (old_inode.i_blocks == 0 || new_inode.i_blocks == 0) {
      continue;
    }

    // Skip inodes that are not the same type
    bool is_old_dir = (ext2fs_check_directory(fs_old, ino) == 0);
    bool is_new_dir = (ext2fs_check_directory(fs_new, ino) == 0);
    if (is_old_dir != is_new_dir) {
      continue;
    }

    // Process the inodes metadata blocks
    // For normal files, metadata blocks are indirect, double indirect
    // and triple indirect blocks (no data blocks). For directories and
    // the journal, all blocks are considered metadata blocks.
    LOG(INFO) << "Processing inode " << ino << " metadata";

    bool all_blocks = ((ino == EXT2_JOURNAL_INO) || is_old_dir || is_new_dir);

    vector<Extent> old_extents;
    error = ext2fs_block_iterate2(fs_old, ino, 0, NULL,
                                  all_blocks ? ProcessInodeAllBlocks :
                                               ProcessInodeMetadataBlocks,
                                  &old_extents);
    if (error) {
      LOG(ERROR) << "Failed to enumerate old inode " << ino
                << " blocks (" << error << ")";
      continue;
    }

    vector<Extent> new_extents;
    error = ext2fs_block_iterate2(fs_new, ino, 0, NULL,
                                  all_blocks ? ProcessInodeAllBlocks :
                                               ProcessInodeMetadataBlocks,
                                  &new_extents);
    if (error) {
      LOG(ERROR) << "Failed to enumerate new inode " << ino
                << " blocks (" << error << ")";
      continue;
    }

    // Skip inode if there are no metadata blocks
    if (old_extents.size() == 0 || new_extents.size() == 0) {
      continue;
    }

    // Make sure the two inodes have the same metadata blocks
    if (old_extents.size() != new_extents.size()) {
      continue;
    }

    bool same_metadata_extents = true;
    vector<Extent>::iterator it_old;
    vector<Extent>::iterator it_new;
    for (it_old = old_extents.begin(),
         it_new = new_extents.begin();
         it_old != old_extents.end() && it_new != new_extents.end();
         it_old++, it_new++) {
      if (it_old->start_block() != it_new->start_block() ||
          it_old->num_blocks() != it_new->num_blocks()) {
            same_metadata_extents = false;
            break;
      }
    }

    if (!same_metadata_extents) {
      continue;
    }

    // We have identical inode metadata blocks, we can now add them to
    // our graph and blocks vector
    string metadata_name = StringPrintf("<fs-inode-%d-metadata>", ino);
    TEST_AND_RETURN_FALSE(AddMetadataExtents(graph,
                                             blocks,
                                             fs_old,
                                             fs_new,
                                             metadata_name,
                                             old_extents,
                                             data_fd,
                                             data_file_size));
  }

  ext2fs_close_inode_scan(iscan);

  return true;
}

}  // namespace {}

// Reads metadata from old image and new image and determines
// the smallest way to encode the metadata for the diff.
// If there's no change in the metadata, it creates a MOVE
// operation. If there is a change, the smallest of REPLACE, REPLACE_BZ,
// or BSDIFF wins. It writes the diff to data_fd and updates data_file_size
// accordingly. It also adds the required operation to the graph and adds the
// metadata extents to blocks.
// Returns true on success.
bool Ext2Metadata::DeltaReadMetadata(Graph* graph,
                                     vector<Block>* blocks,
                                     const string& old_image,
                                     const string& new_image,
                                     int data_fd,
                                     off_t* data_file_size) {
  // Open the two file systems.
  ext2_filsys fs_old;
  TEST_AND_RETURN_FALSE_ERRCODE(ext2fs_open(old_image.c_str(), 0, 0, 0,
                                            unix_io_manager, &fs_old));
  ScopedExt2fsCloser fs_old_closer(fs_old);

  ext2_filsys fs_new;
  TEST_AND_RETURN_FALSE_ERRCODE(ext2fs_open(new_image.c_str(), 0, 0, 0,
                                            unix_io_manager, &fs_new));
  ScopedExt2fsCloser fs_new_closer(fs_new);

  // Make sure these two file systems are the same.
  // If they are not the same, the metadata blocks will be packaged up in its
  // entirety by ReadUnwrittenBlocks().
  if (fs_old->blocksize != fs_new->blocksize ||
      fs_old->fragsize != fs_new->fragsize ||
      fs_old->group_desc_count != fs_new->group_desc_count ||
      fs_old->inode_blocks_per_group != fs_new->inode_blocks_per_group ||
      fs_old->super->s_inodes_count != fs_new->super->s_inodes_count ||
      fs_old->super->s_blocks_count != fs_new->super->s_blocks_count) {
    return true;
  }

  // Process the main file system metadata (superblock, inode tables, etc)
  TEST_AND_RETURN_FALSE(ReadFilesystemMetadata(graph,
                                               blocks,
                                               fs_old,
                                               fs_new,
                                               data_fd,
                                               data_file_size));

  // Process each inode metadata blocks.
  TEST_AND_RETURN_FALSE(ReadInodeMetadata(graph,
                                          blocks,
                                          fs_old,
                                          fs_new,
                                          data_fd,
                                          data_file_size));

  return true;
}

};  // namespace chromeos_update_engine
