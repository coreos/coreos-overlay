// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/delta_diff_generator.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <bzlib.h>
#include "base/logging.h"
#include "update_engine/bzip.h"
#include "update_engine/cycle_breaker.h"
#include "update_engine/extent_mapper.h"
#include "update_engine/file_writer.h"
#include "update_engine/filesystem_iterator.h"
#include "update_engine/graph_types.h"
#include "update_engine/graph_utils.h"
#include "update_engine/payload_signer.h"
#include "update_engine/subprocess.h"
#include "update_engine/topological_sort.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

using std::make_pair;
using std::max;
using std::min;
using std::set;
using std::string;
using std::vector;

namespace chromeos_update_engine {

typedef DeltaDiffGenerator::Block Block;

namespace {
const size_t kBlockSize = 4096;
const size_t kRootFSPartitionSize = 1 * 1024 * 1024 * 1024;  // 1 GiB
const uint64_t kVersionNumber = 1;

// Stores all Extents for a file into 'out'. Returns true on success.
bool GatherExtents(const string& path,
                   google::protobuf::RepeatedPtrField<Extent>* out) {
  vector<Extent> extents;
  TEST_AND_RETURN_FALSE(extent_mapper::ExtentsForFileFibmap(path, &extents));
  DeltaDiffGenerator::StoreExtents(extents, out);
  return true;
}

// Runs the bsdiff tool on two files and returns the resulting delta in
// 'out'. Returns true on success.
bool BsdiffFiles(const string& old_file,
                 const string& new_file,
                 vector<char>* out) {
  const string kPatchFile = "/tmp/delta.patchXXXXXX";
  string patch_file_path;

  TEST_AND_RETURN_FALSE(
      utils::MakeTempFile(kPatchFile, &patch_file_path, NULL));

  vector<string> cmd;
  cmd.push_back(kBsdiffPath);
  cmd.push_back(old_file);
  cmd.push_back(new_file);
  cmd.push_back(patch_file_path);

  int rc = 1;
  vector<char> patch_file;
  TEST_AND_RETURN_FALSE(Subprocess::SynchronousExec(cmd, &rc));
  TEST_AND_RETURN_FALSE(rc == 0);
  TEST_AND_RETURN_FALSE(utils::ReadFile(patch_file_path, out));
  unlink(patch_file_path.c_str());
  return true;
}

// The blocks vector contains a reader and writer for each block on the
// filesystem that's being in-place updated. We populate the reader/writer
// fields of blocks by calling this function.
// For each block in 'operation' that is read or written, find that block
// in 'blocks' and set the reader/writer field to the vertex passed.
// 'graph' is not strictly necessary, but useful for printing out
// error messages.
bool AddInstallOpToBlocksVector(
    const DeltaArchiveManifest_InstallOperation& operation,
    vector<Block>* blocks,
    const Graph& graph,
    Vertex::Index vertex) {
  LOG(INFO) << "AddInstallOpToBlocksVector(" << vertex << "), "
            << graph[vertex].file_name;
  // See if this is already present.
  TEST_AND_RETURN_FALSE(operation.dst_extents_size() > 0);

  enum BlockField { READER = 0, WRITER, BLOCK_FIELD_COUNT };
  for (int field = READER; field < BLOCK_FIELD_COUNT; field++) {
    const int extents_size =
        (field == READER) ? operation.src_extents_size() :
        operation.dst_extents_size();
    const char* past_participle = (field == READER) ? "read" : "written";
    const google::protobuf::RepeatedPtrField<Extent>& extents =
        (field == READER) ? operation.src_extents() : operation.dst_extents();
    Vertex::Index Block::*access_type =
        (field == READER) ? &Block::reader : &Block::writer;

    for (int i = 0; i < extents_size; i++) {
      const Extent& extent = extents.Get(i);
      if (extent.start_block() == kSparseHole) {
        // Hole in sparse file. skip
        continue;
      }
      for (uint64_t block = extent.start_block();
           block < (extent.start_block() + extent.num_blocks()); block++) {
        LOG(INFO) << "ext: " << i << " block: " << block;
        if ((*blocks)[block].*access_type != Vertex::kInvalidIndex) {
          LOG(FATAL) << "Block " << block << " is already "
                     << past_participle << " by "
                     << (*blocks)[block].*access_type << "("
                     << graph[(*blocks)[block].*access_type].file_name
                     << ") and also " << vertex << "("
                     << graph[vertex].file_name << ")";
        }
        (*blocks)[block].*access_type = vertex;
      }
    }
  }
  return true;
}

// For a given regular file which must exist at new_root + path, and may
// exist at old_root + path, creates a new InstallOperation and adds it to
// the graph. Also, populates the 'blocks' array as necessary.
// Also, writes the data necessary to send the file down to the client
// into data_fd, which has length *data_file_size. *data_file_size is
// updated appropriately.
// Returns true on success.
bool DeltaReadFile(Graph* graph,
                   vector<Block>* blocks,
                   const string& old_root,
                   const string& new_root,
                   const string& path,  // within new_root
                   int data_fd,
                   off_t* data_file_size) {
  vector<char> data;
  DeltaArchiveManifest_InstallOperation operation;

  TEST_AND_RETURN_FALSE(DeltaDiffGenerator::ReadFileToDiff(old_root + path,
                                                           new_root + path,
                                                           &data,
                                                           &operation));

  // Write the data
  if (operation.type() != DeltaArchiveManifest_InstallOperation_Type_MOVE) {
    operation.set_data_offset(*data_file_size);
    operation.set_data_length(data.size());
  }

  TEST_AND_RETURN_FALSE(utils::WriteAll(data_fd, &data[0], data.size()));
  *data_file_size += data.size();

  // Now, insert into graph and blocks vector
  graph->resize(graph->size() + 1);
  graph->back().op = operation;
  CHECK(graph->back().op.has_type());
  graph->back().file_name = path;

  TEST_AND_RETURN_FALSE(AddInstallOpToBlocksVector(graph->back().op,
                                                   blocks,
                                                   *graph,
                                                   graph->size() - 1));
  return true;
}

// For each regular file within new_root, creates a node in the graph,
// determines the best way to compress it (REPLACE, REPLACE_BZ, COPY, BSDIFF),
// and writes any necessary data to the end of data_fd.
bool DeltaReadFiles(Graph* graph,
                    vector<Block>* blocks,
                    const string& old_root,
                    const string& new_root,
                    int data_fd,
                    off_t* data_file_size) {
  set<ino_t> visited_inodes;
  for (FilesystemIterator fs_iter(new_root,
                                  utils::SetWithValue<string>("/lost+found"));
       !fs_iter.IsEnd(); fs_iter.Increment()) {
    if (!S_ISREG(fs_iter.GetStat().st_mode))
      continue;

    // Make sure we visit each inode only once.
    if (utils::SetContainsKey(visited_inodes, fs_iter.GetStat().st_ino))
      continue;
    visited_inodes.insert(fs_iter.GetStat().st_ino);
    if (fs_iter.GetStat().st_size == 0)
      continue;

    LOG(INFO) << "Encoding file " << fs_iter.GetPartialPath();

    TEST_AND_RETURN_FALSE(DeltaReadFile(graph,
                                        blocks,
                                        old_root,
                                        new_root,
                                        fs_iter.GetPartialPath(),
                                        data_fd,
                                        data_file_size));
  }
  return true;
}

// Attempts to find |block_count| blocks to use as scratch space. Returns true
// on success. Right now we return exactly as many blocks as are required.
//
// TODO(adlr): Consider returning all scratch blocks, even if there are extras,
// to make it easier for a scratch allocator to find contiguous regions for
// specific scratch writes.
bool FindScratchSpace(const vector<Block>& blocks,
                      vector<Block>::size_type block_count,
                      vector<Extent>* out) {
  // Scan |blocks| for blocks that are neither read, nor written. If we don't
  // find enough of those, look past the end of |blocks| till the end of the
  // partition. If we don't find |block_count| scratch blocks, return false.
  //
  // TODO(adlr): Return blocks that are written by operations that don't have
  // incoming edges (and thus, can be deferred until all old blocks are read by
  // other operations).
  vector<Extent> ret;
  vector<Block>::size_type blocks_found = 0;
  const size_t kPartitionBlocks = kRootFSPartitionSize / kBlockSize;
  for (vector<Block>::size_type i = 0;
       i < kPartitionBlocks && blocks_found < block_count; i++) {
    if (i >= blocks.size() ||
        (blocks[i].reader == Vertex::kInvalidIndex &&
         blocks[i].writer == Vertex::kInvalidIndex)) {
      graph_utils::AppendBlockToExtents(&ret, i);
      blocks_found++;
    }
  }
  LOG(INFO) << "found " << blocks_found << " scratch blocks";
  if (blocks_found == block_count) {
    out->swap(ret);
    return true;
  }
  return false;
}

// This class takes a collection of Extents and allows the client to
// allocate space from these extents. The client must not request more
// space then exists in the source extents. Space is allocated from the
// beginning of the source extents on; no consideration is paid to
// fragmentation.
class LinearExtentAllocator {
 public:
  explicit LinearExtentAllocator(const vector<Extent>& extents)
      : extents_(extents),
        extent_index_(0),
        extent_blocks_allocated_(0) {}
  vector<Extent> Allocate(const uint64_t block_count) {
    vector<Extent> ret;
    for (uint64_t blocks = 0; blocks < block_count; blocks++) {
      CHECK_LT(extent_index_, extents_.size());
      CHECK_LT(extent_blocks_allocated_, extents_[extent_index_].num_blocks());
      graph_utils::AppendBlockToExtents(
          &ret,
          extents_[extent_index_].start_block() + extent_blocks_allocated_);
      extent_blocks_allocated_++;
      if (extent_blocks_allocated_ >= extents_[extent_index_].num_blocks()) {
        extent_blocks_allocated_ = 0;
        extent_index_++;
      }
    }
    return ret;
  }
 private:
  const vector<Extent> extents_;
  vector<Extent>::size_type extent_index_;  // current Extent
  // number of blocks allocated from the current extent
  uint64_t extent_blocks_allocated_;
};

// Reads blocks from image_path that are not yet marked as being written
// in the blocks array. These blocks that remain are non-file-data blocks.
// In the future we might consider intelligent diffing between this data
// and data in the previous image, but for now we just bzip2 compress it
// and include it in the update.
// Creates a new node in the graph to write these blocks and writes the
// appropriate blob to blobs_fd. Reads and updates blobs_length;
bool ReadUnwrittenBlocks(const vector<Block>& blocks,
                         int blobs_fd,
                         off_t* blobs_length,
                         const string& image_path,
                         DeltaArchiveManifest_InstallOperation* out_op) {
  int image_fd = open(image_path.c_str(), O_RDONLY, 000);
  TEST_AND_RETURN_FALSE_ERRNO(image_fd >= 0);
  ScopedFdCloser image_fd_closer(&image_fd);

  string temp_file_path;
  TEST_AND_RETURN_FALSE(utils::MakeTempFile("/tmp/CrAU_temp_data.XXXXXX",
                                            &temp_file_path,
                                            NULL));

  FILE* file = fopen(temp_file_path.c_str(), "w");
  TEST_AND_RETURN_FALSE(file);
  int err = BZ_OK;

  BZFILE* bz_file = BZ2_bzWriteOpen(&err,
                                    file,
                                    9,  // max compression
                                    0,  // verbosity
                                    0);  // default work factor
  TEST_AND_RETURN_FALSE(err == BZ_OK);

  vector<Extent> extents;
  vector<Block>::size_type block_count = 0;

  LOG(INFO) << "Appending left over blocks to extents";
  for (vector<Block>::size_type i = 0; i < blocks.size(); i++) {
    if (blocks[i].writer != Vertex::kInvalidIndex)
      continue;
    graph_utils::AppendBlockToExtents(&extents, i);
    block_count++;
  }

  // Code will handle 'buf' at any size that's a multiple of kBlockSize,
  // so we arbitrarily set it to 1024 * kBlockSize.
  vector<char> buf(1024 * kBlockSize);

  LOG(INFO) << "Reading left over blocks";
  vector<Block>::size_type blocks_copied_count = 0;

  // For each extent in extents, write the data into BZ2_bzWrite which
  // sends it to an output file.
  // We use the temporary buffer 'buf' to hold the data, which may be
  // smaller than the extent, so in that case we have to loop to get
  // the extent's data (that's the inner while loop).
  for (vector<Extent>::const_iterator it = extents.begin();
       it != extents.end(); ++it) {
    vector<Block>::size_type blocks_read = 0;
    while (blocks_read < it->num_blocks()) {
      const int copy_block_cnt =
          min(buf.size() / kBlockSize,
              static_cast<vector<char>::size_type>(
                  it->num_blocks() - blocks_read));
      ssize_t rc = pread(image_fd,
                         &buf[0],
                         copy_block_cnt * kBlockSize,
                         (it->start_block() + blocks_read) * kBlockSize);
      TEST_AND_RETURN_FALSE_ERRNO(rc >= 0);
      TEST_AND_RETURN_FALSE(static_cast<size_t>(rc) ==
                            copy_block_cnt * kBlockSize);
      BZ2_bzWrite(&err, bz_file, &buf[0], copy_block_cnt * kBlockSize);
      TEST_AND_RETURN_FALSE(err == BZ_OK);
      blocks_read += copy_block_cnt;
      blocks_copied_count += copy_block_cnt;
      LOG(INFO) << "progress: " << ((float)blocks_copied_count)/block_count;
    }
  }
  BZ2_bzWriteClose(&err, bz_file, 0, NULL, NULL);
  TEST_AND_RETURN_FALSE(err == BZ_OK);
  bz_file = NULL;
  TEST_AND_RETURN_FALSE_ERRNO(0 == fclose(file));
  file = NULL;

  vector<char> compressed_data;
  LOG(INFO) << "Reading compressed data off disk";
  TEST_AND_RETURN_FALSE(utils::ReadFile(temp_file_path, &compressed_data));
  TEST_AND_RETURN_FALSE(unlink(temp_file_path.c_str()) == 0);

  // Add node to graph to write these blocks
  out_op->set_type(DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ);
  out_op->set_data_offset(*blobs_length);
  out_op->set_data_length(compressed_data.size());
  *blobs_length += compressed_data.size();
  out_op->set_dst_length(kBlockSize * block_count);
  DeltaDiffGenerator::StoreExtents(extents, out_op->mutable_dst_extents());

  TEST_AND_RETURN_FALSE(utils::WriteAll(blobs_fd,
                                        &compressed_data[0],
                                        compressed_data.size()));
  LOG(INFO) << "done with extra blocks";
  return true;
}

// Writes the uint64_t passed in in host-endian to the file as big-endian.
// Returns true on success.
bool WriteUint64AsBigEndian(FileWriter* writer, const uint64_t value) {
  uint64_t value_be = htobe64(value);
  TEST_AND_RETURN_FALSE(writer->Write(&value_be, sizeof(value_be)) ==
                        sizeof(value_be));
  return true;
}

// Adds each operation from the graph to the manifest in the order
// specified by 'order'.
void InstallOperationsToManifest(
    const Graph& graph,
    const vector<Vertex::Index>& order,
    const vector<DeltaArchiveManifest_InstallOperation>& kernel_ops,
    DeltaArchiveManifest* out_manifest) {
  for (vector<Vertex::Index>::const_iterator it = order.begin();
       it != order.end(); ++it) {
    DeltaArchiveManifest_InstallOperation* op =
        out_manifest->add_install_operations();
    *op = graph[*it].op;
  }
  for (vector<DeltaArchiveManifest_InstallOperation>::const_iterator it =
           kernel_ops.begin(); it != kernel_ops.end(); ++it) {
    DeltaArchiveManifest_InstallOperation* op =
        out_manifest->add_kernel_install_operations();
    *op = *it;
  }
}

void CheckGraph(const Graph& graph) {
  for (Graph::const_iterator it = graph.begin(); it != graph.end(); ++it) {
    CHECK(it->op.has_type());
  }
}

// Delta compresses a kernel partition new_kernel_part with knowledge of
// the old kernel partition old_kernel_part.
bool DeltaCompressKernelPartition(
    const string& old_kernel_part,
    const string& new_kernel_part,
    vector<DeltaArchiveManifest_InstallOperation>* ops,
    int blobs_fd,
    off_t* blobs_length) {
  // For now, just bsdiff the kernel partition as a whole.
  // TODO(adlr): Use knowledge of how the kernel partition is laid out
  // to more efficiently compress it.

  LOG(INFO) << "Delta compressing kernel partition...";

  // Add a new install operation
  ops->resize(1);
  DeltaArchiveManifest_InstallOperation* op = &(*ops)[0];
  op->set_type(DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ);
  op->set_data_offset(*blobs_length);

  // Do the actual compression
  vector<char> data;
  TEST_AND_RETURN_FALSE(utils::ReadFile(new_kernel_part, &data));
  TEST_AND_RETURN_FALSE(!data.empty());

  vector<char> data_bz;
  TEST_AND_RETURN_FALSE(BzipCompress(data, &data_bz));
  CHECK(!data_bz.empty());

  TEST_AND_RETURN_FALSE(utils::WriteAll(blobs_fd, &data_bz[0], data_bz.size()));
  *blobs_length += data_bz.size();

  off_t new_part_size = utils::FileSize(new_kernel_part);
  TEST_AND_RETURN_FALSE(new_part_size >= 0);

  op->set_data_length(data_bz.size());

  op->set_dst_length(new_part_size);

  // There's a single dest extent
  Extent* dst_extent = op->add_dst_extents();
  dst_extent->set_start_block(0);
  dst_extent->set_num_blocks((new_part_size + kBlockSize - 1) / kBlockSize);

  LOG(INFO) << "Done compressing kernel partition.";
  return true;
}

}  // namespace {}

bool DeltaDiffGenerator::ReadFileToDiff(
    const string& old_filename,
    const string& new_filename,
    vector<char>* out_data,
    DeltaArchiveManifest_InstallOperation* out_op) {
  // Read new data in
  vector<char> new_data;
  TEST_AND_RETURN_FALSE(utils::ReadFile(new_filename, &new_data));

  TEST_AND_RETURN_FALSE(!new_data.empty());

  vector<char> new_data_bz;
  TEST_AND_RETURN_FALSE(BzipCompress(new_data, &new_data_bz));
  CHECK(!new_data_bz.empty());

  vector<char> data;  // Data blob that will be written to delta file.

  DeltaArchiveManifest_InstallOperation operation;
  size_t current_best_size = 0;
  if (new_data.size() <= new_data_bz.size()) {
    operation.set_type(DeltaArchiveManifest_InstallOperation_Type_REPLACE);
    current_best_size = new_data.size();
    data = new_data;
  } else {
    operation.set_type(DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ);
    current_best_size = new_data_bz.size();
    data = new_data_bz;
  }

  // Do we have an original file to consider?
  struct stat old_stbuf;
  if (0 != stat(old_filename.c_str(), &old_stbuf)) {
    // If stat-ing the old file fails, it should be because it doesn't exist.
    TEST_AND_RETURN_FALSE(errno == ENOTDIR || errno == ENOENT);
  } else {
    // Read old data
    vector<char> old_data;
    TEST_AND_RETURN_FALSE(utils::ReadFile(old_filename, &old_data));
    if (old_data == new_data) {
      // No change in data.
      operation.set_type(DeltaArchiveManifest_InstallOperation_Type_MOVE);
      current_best_size = 0;
      data.clear();
    } else {
      // Try bsdiff of old to new data
      vector<char> bsdiff_delta;
      TEST_AND_RETURN_FALSE(
          BsdiffFiles(old_filename, new_filename, &bsdiff_delta));
      CHECK_GT(bsdiff_delta.size(), 0);
      if (bsdiff_delta.size() < current_best_size) {
        operation.set_type(DeltaArchiveManifest_InstallOperation_Type_BSDIFF);
        current_best_size = bsdiff_delta.size();

        data = bsdiff_delta;
      }
    }
  }

  // Set parameters of the operations
  CHECK_EQ(data.size(), current_best_size);

  if (operation.type() == DeltaArchiveManifest_InstallOperation_Type_MOVE ||
      operation.type() == DeltaArchiveManifest_InstallOperation_Type_BSDIFF) {
    TEST_AND_RETURN_FALSE(
        GatherExtents(old_filename, operation.mutable_src_extents()));
    operation.set_src_length(old_stbuf.st_size);
  }

  TEST_AND_RETURN_FALSE(
      GatherExtents(new_filename, operation.mutable_dst_extents()));
  operation.set_dst_length(new_data.size());

  out_data->swap(data);
  *out_op = operation;

  return true;
}

void DeltaDiffGenerator::SubstituteBlocks(
    DeltaArchiveManifest_InstallOperation* op,
    const vector<Extent>& remove_extents,
    const vector<Extent>& replace_extents) {
  // First, expand out the blocks that op reads from
  vector<uint64_t> read_blocks;
  for (int i = 0; i < op->src_extents_size(); i++) {
    const Extent& extent = op->src_extents(i);
    if (extent.start_block() == kSparseHole) {
      read_blocks.resize(read_blocks.size() + extent.num_blocks(), kSparseHole);
    } else {
      for (uint64_t block = extent.start_block();
           block < (extent.start_block() + extent.num_blocks()); block++) {
        read_blocks.push_back(block);
      }
    }
  }
  {
    // Expand remove_extents and replace_extents
    vector<uint64_t> remove_extents_expanded;
    for (vector<Extent>::const_iterator it = remove_extents.begin();
         it != remove_extents.end(); ++it) {
      const Extent& extent = *it;
      for (uint64_t block = extent.start_block();
           block < (extent.start_block() + extent.num_blocks()); block++) {
        remove_extents_expanded.push_back(block);
      }
    }
    vector<uint64_t> replace_extents_expanded;
    for (vector<Extent>::const_iterator it = replace_extents.begin();
         it != replace_extents.end(); ++it) {
      const Extent& extent = *it;
      for (uint64_t block = extent.start_block();
           block < (extent.start_block() + extent.num_blocks()); block++) {
        replace_extents_expanded.push_back(block);
      }
    }
    CHECK_EQ(remove_extents_expanded.size(), replace_extents_expanded.size());
    for (vector<uint64_t>::size_type i = 0;
         i < replace_extents_expanded.size(); i++) {
      vector<uint64_t>::size_type index = 0;
      CHECK(utils::VectorIndexOf(read_blocks,
                                 remove_extents_expanded[i],
                                 &index));
      CHECK(read_blocks[index] == remove_extents_expanded[i]);
      read_blocks[index] = replace_extents_expanded[i];
    }
  }
  // Convert read_blocks back to extents
  op->clear_src_extents();
  vector<Extent> new_extents;
  for (vector<uint64_t>::const_iterator it = read_blocks.begin();
       it != read_blocks.end(); ++it) {
    graph_utils::AppendBlockToExtents(&new_extents, *it);
  }
  DeltaDiffGenerator::StoreExtents(new_extents, op->mutable_src_extents());
}

bool DeltaDiffGenerator::CutEdges(Graph* graph,
                                  const vector<Block>& blocks,
                                  const set<Edge>& edges) {
  // First, find enough scratch space for the edges we'll be cutting.
  vector<Block>::size_type blocks_required = 0;
  for (set<Edge>::const_iterator it = edges.begin(); it != edges.end(); ++it) {
    blocks_required += graph_utils::EdgeWeight(*graph, *it);
  }
  vector<Extent> scratch_extents;
  LOG(INFO) << "requesting " << blocks_required << " blocks of scratch";
  TEST_AND_RETURN_FALSE(
      FindScratchSpace(blocks, blocks_required, &scratch_extents));
  LinearExtentAllocator scratch_allocator(scratch_extents);

  uint64_t scratch_blocks_used = 0;
  for (set<Edge>::const_iterator it = edges.begin();
       it != edges.end(); ++it) {
    vector<Extent> old_extents =
        (*graph)[it->first].out_edges[it->second].extents;
    // Choose some scratch space
    scratch_blocks_used += graph_utils::EdgeWeight(*graph, *it);
    LOG(INFO) << "using " << graph_utils::EdgeWeight(*graph, *it)
              << " scratch blocks ("
              << scratch_blocks_used << ")";
    vector<Extent> scratch =
        scratch_allocator.Allocate(graph_utils::EdgeWeight(*graph, *it));
    // create vertex to copy original->scratch
    graph->resize(graph->size() + 1);

    // make node depend on the copy operation
    (*graph)[it->first].out_edges.insert(make_pair(graph->size() - 1,
                                                   EdgeProperties()));

    // Set src/dst extents and other proto variables for copy operation
    graph->back().op.set_type(DeltaArchiveManifest_InstallOperation_Type_MOVE);
    DeltaDiffGenerator::StoreExtents(
        (*graph)[it->first].out_edges[it->second].extents,
        graph->back().op.mutable_src_extents());
    DeltaDiffGenerator::StoreExtents(scratch,
                                     graph->back().op.mutable_dst_extents());
    graph->back().op.set_src_length(
        graph_utils::EdgeWeight(*graph, *it) * kBlockSize);
    graph->back().op.set_dst_length(graph->back().op.src_length());

    // make the dest node read from the scratch space
    DeltaDiffGenerator::SubstituteBlocks(
        &((*graph)[it->second].op),
        (*graph)[it->first].out_edges[it->second].extents,
        scratch);

    // delete the old edge
    CHECK_EQ(1, (*graph)[it->first].out_edges.erase(it->second));

    // Add an edge from dst to copy operation
    (*graph)[it->second].out_edges.insert(make_pair(graph->size() - 1,
                                                    EdgeProperties()));
  }
  return true;
}

// Stores all Extents in 'extents' into 'out'.
void DeltaDiffGenerator::StoreExtents(
    vector<Extent>& extents,
    google::protobuf::RepeatedPtrField<Extent>* out) {
  for (vector<Extent>::const_iterator it = extents.begin();
       it != extents.end(); ++it) {
    Extent* new_extent = out->Add();
    *new_extent = *it;
  }
}

// Creates all the edges for the graph. Writers of a block point to
// readers of the same block. This is because for an edge A->B, B
// must complete before A executes.
void DeltaDiffGenerator::CreateEdges(Graph* graph,
                                     const vector<Block>& blocks) {
  for (vector<Block>::size_type i = 0; i < blocks.size(); i++) {
    // Blocks with both a reader and writer get an edge
    if (blocks[i].reader == Vertex::kInvalidIndex ||
        blocks[i].writer == Vertex::kInvalidIndex)
      continue;
    // Don't have a node depend on itself
    if (blocks[i].reader == blocks[i].writer)
      continue;
    // See if there's already an edge we can add onto
    Vertex::EdgeMap::iterator edge_it =
        (*graph)[blocks[i].writer].out_edges.find(blocks[i].reader);
    if (edge_it == (*graph)[blocks[i].writer].out_edges.end()) {
      // No existing edge. Create one
      (*graph)[blocks[i].writer].out_edges.insert(
          make_pair(blocks[i].reader, EdgeProperties()));
      edge_it = (*graph)[blocks[i].writer].out_edges.find(blocks[i].reader);
      CHECK(edge_it != (*graph)[blocks[i].writer].out_edges.end());
    }
    graph_utils::AppendBlockToExtents(&edge_it->second.extents, i);
  }
}

bool DeltaDiffGenerator::ReorderDataBlobs(
    DeltaArchiveManifest* manifest,
    const std::string& data_blobs_path,
    const std::string& new_data_blobs_path) {
  int in_fd = open(data_blobs_path.c_str(), O_RDONLY, 0);
  TEST_AND_RETURN_FALSE_ERRNO(in_fd >= 0);
  ScopedFdCloser in_fd_closer(&in_fd);

  DirectFileWriter writer;
  TEST_AND_RETURN_FALSE(
      writer.Open(new_data_blobs_path.c_str(),
                  O_WRONLY | O_TRUNC | O_CREAT,
                  0644) == 0);
  ScopedFileWriterCloser writer_closer(&writer);
  uint64_t out_file_size = 0;

  for (int i = 0; i < (manifest->install_operations_size() +
                       manifest->kernel_install_operations_size()); i++) {
    DeltaArchiveManifest_InstallOperation* op = NULL;
    if (i < manifest->install_operations_size()) {
      op = manifest->mutable_install_operations(i);
    } else {
      op = manifest->mutable_kernel_install_operations(
          i - manifest->install_operations_size());
    }
    if (!op->has_data_offset())
      continue;
    CHECK(op->has_data_length());
    vector<char> buf(op->data_length());
    ssize_t rc = pread(in_fd, &buf[0], buf.size(), op->data_offset());
    TEST_AND_RETURN_FALSE(rc == static_cast<ssize_t>(buf.size()));

    op->set_data_offset(out_file_size);
    TEST_AND_RETURN_FALSE(writer.Write(&buf[0], buf.size()) ==
                          static_cast<ssize_t>(buf.size()));
    out_file_size += buf.size();
  }
  return true;
}

bool DeltaDiffGenerator::GenerateDeltaUpdateFile(
    const string& old_root,
    const string& old_image,
    const string& new_root,
    const string& new_image,
    const string& old_kernel_part,
    const string& new_kernel_part,
    const string& output_path,
    const string& private_key_path) {
  struct stat old_image_stbuf;
  TEST_AND_RETURN_FALSE_ERRNO(stat(old_image.c_str(), &old_image_stbuf) == 0);
  struct stat new_image_stbuf;
  TEST_AND_RETURN_FALSE_ERRNO(stat(new_image.c_str(), &new_image_stbuf) == 0);
  LOG_IF(WARNING, new_image_stbuf.st_size != old_image_stbuf.st_size)
      << "Old and new images are different sizes.";
  LOG_IF(FATAL, new_image_stbuf.st_size % kBlockSize)
      << "New image not a multiple of block size " << kBlockSize;
  LOG_IF(FATAL, old_image_stbuf.st_size % kBlockSize)
      << "Old image not a multiple of block size " << kBlockSize;

  // Sanity check kernel partition args
  TEST_AND_RETURN_FALSE(utils::FileSize(old_kernel_part) >= 0);
  TEST_AND_RETURN_FALSE(utils::FileSize(new_kernel_part) >= 0);

  vector<Block> blocks(max(old_image_stbuf.st_size / kBlockSize,
                           new_image_stbuf.st_size / kBlockSize));
  LOG(INFO) << "invalid: " << Vertex::kInvalidIndex;
  LOG(INFO) << "len: " << blocks.size();
  for (vector<Block>::size_type i = 0; i < blocks.size(); i++) {
    CHECK(blocks[i].reader == Vertex::kInvalidIndex);
    CHECK(blocks[i].writer == Vertex::kInvalidIndex);
  }
  Graph graph;
  CheckGraph(graph);

  const string kTempFileTemplate("/tmp/CrAU_temp_data.XXXXXX");
  string temp_file_path;
  off_t data_file_size = 0;

  LOG(INFO) << "Reading files...";

  vector<DeltaArchiveManifest_InstallOperation> kernel_ops;

  DeltaArchiveManifest_InstallOperation final_op;
  {
    int fd;
    TEST_AND_RETURN_FALSE(
        utils::MakeTempFile(kTempFileTemplate, &temp_file_path, &fd));
    TEST_AND_RETURN_FALSE(fd >= 0);
    ScopedFdCloser fd_closer(&fd);

    TEST_AND_RETURN_FALSE(DeltaReadFiles(&graph,
                                         &blocks,
                                         old_root,
                                         new_root,
                                         fd,
                                         &data_file_size));
    CheckGraph(graph);

    TEST_AND_RETURN_FALSE(ReadUnwrittenBlocks(blocks,
                                              fd,
                                              &data_file_size,
                                              new_image,
                                              &final_op));

    // Read kernel partition
    TEST_AND_RETURN_FALSE(DeltaCompressKernelPartition(old_kernel_part,
                                                       new_kernel_part,
                                                       &kernel_ops,
                                                       fd,
                                                       &data_file_size));
  }
  CheckGraph(graph);

  LOG(INFO) << "Creating edges...";
  CreateEdges(&graph, blocks);
  CheckGraph(graph);

  CycleBreaker cycle_breaker;
  LOG(INFO) << "Finding cycles...";
  set<Edge> cut_edges;
  cycle_breaker.BreakCycles(graph, &cut_edges);
  CheckGraph(graph);

  // Calculate number of scratch blocks needed

  LOG(INFO) << "Cutting cycles...";
  TEST_AND_RETURN_FALSE(CutEdges(&graph, blocks, cut_edges));
  CheckGraph(graph);

  vector<Vertex::Index> final_order;
  LOG(INFO) << "Ordering...";
  TopologicalSort(graph, &final_order);
  CheckGraph(graph);

  // Convert to protobuf Manifest object
  DeltaArchiveManifest manifest;
  CheckGraph(graph);
  InstallOperationsToManifest(graph, final_order, kernel_ops, &manifest);
  {
    // Write final operation
    DeltaArchiveManifest_InstallOperation* op =
        manifest.add_install_operations();
    *op = final_op;
    CHECK(op->has_type());
    LOG(INFO) << "final op length: " << op->data_length();
  }

  CheckGraph(graph);
  manifest.set_block_size(kBlockSize);

  // Reorder the data blobs with the newly ordered manifest
  string ordered_blobs_path;
  TEST_AND_RETURN_FALSE(utils::MakeTempFile(
      "/tmp/CrAU_temp_data.ordered.XXXXXX",
      &ordered_blobs_path,
      false));
  TEST_AND_RETURN_FALSE(ReorderDataBlobs(&manifest,
                                         temp_file_path,
                                         ordered_blobs_path));

  // Check that install op blobs are in order and that all blocks are written.
  uint64_t next_blob_offset = 0;
  {
    vector<uint32_t> written_count(blocks.size(), 0);
    for (int i = 0; i < (manifest.install_operations_size() +
                         manifest.kernel_install_operations_size()); i++) {
      DeltaArchiveManifest_InstallOperation* op =
          i < manifest.install_operations_size() ?
          manifest.mutable_install_operations(i) :
          manifest.mutable_kernel_install_operations(
              i - manifest.install_operations_size());
      for (int j = 0; j < op->dst_extents_size(); j++) {
        const Extent& extent = op->dst_extents(j);
        for (uint64_t block = extent.start_block();
             block < (extent.start_block() + extent.num_blocks()); block++) {
          if (block < blocks.size())
            written_count[block]++;
        }
      }
      if (op->has_data_offset()) {
        if (op->data_offset() != next_blob_offset) {
          LOG(FATAL) << "bad blob offset! " << op->data_offset() << " != "
                     << next_blob_offset;
        }
        next_blob_offset += op->data_length();
      }
    }
    // check all blocks written to
    for (vector<uint32_t>::size_type i = 0; i < written_count.size(); i++) {
      if (written_count[i] == 0) {
        LOG(FATAL) << "block " << i << " not written!";
      }
    }
  }

  // Signatures appear at the end of the blobs. Note the offset in the
  // manifest
  if (!private_key_path.empty()) {
    LOG(INFO) << "Making room for signature in file";
    manifest.set_signatures_offset(next_blob_offset);
    LOG(INFO) << "set? " << manifest.has_signatures_offset();
    // Add a dummy op at the end to appease older clients
    DeltaArchiveManifest_InstallOperation* dummy_op =
        manifest.add_kernel_install_operations();
    dummy_op->set_type(DeltaArchiveManifest_InstallOperation_Type_REPLACE);
    dummy_op->set_data_offset(next_blob_offset);
    manifest.set_signatures_offset(next_blob_offset);
    uint64_t signature_blob_length = 0;
    TEST_AND_RETURN_FALSE(
        PayloadSigner::SignatureBlobLength(private_key_path,
                                           &signature_blob_length));
    dummy_op->set_data_length(signature_blob_length);
    manifest.set_signatures_size(signature_blob_length);
    Extent* dummy_extent = dummy_op->add_dst_extents();
    // Tell the dummy op to write this data to a big sparse hole
    dummy_extent->set_start_block(kSparseHole);
    dummy_extent->set_num_blocks((signature_blob_length + kBlockSize - 1) /
                                 kBlockSize);
  }

  // Serialize protobuf
  string serialized_manifest;

  CheckGraph(graph);
  TEST_AND_RETURN_FALSE(manifest.AppendToString(&serialized_manifest));
  CheckGraph(graph);

  LOG(INFO) << "Writing final delta file header...";
  DirectFileWriter writer;
  TEST_AND_RETURN_FALSE_ERRNO(writer.Open(output_path.c_str(),
                                          O_WRONLY | O_CREAT | O_TRUNC,
                                          0644) == 0);
  ScopedFileWriterCloser writer_closer(&writer);

  // Write header
  TEST_AND_RETURN_FALSE(writer.Write(kDeltaMagic, strlen(kDeltaMagic)) ==
                        static_cast<ssize_t>(strlen(kDeltaMagic)));

  // Write version number
  TEST_AND_RETURN_FALSE(WriteUint64AsBigEndian(&writer, kVersionNumber));

  // Write protobuf length
  TEST_AND_RETURN_FALSE(WriteUint64AsBigEndian(&writer,
                                               serialized_manifest.size()));

  // Write protobuf
  LOG(INFO) << "Writing final delta file protobuf... "
            << serialized_manifest.size();
  TEST_AND_RETURN_FALSE(writer.Write(serialized_manifest.data(),
                                     serialized_manifest.size()) ==
                        static_cast<ssize_t>(serialized_manifest.size()));

  // Append the data blobs
  LOG(INFO) << "Writing final delta file data blobs...";
  int blobs_fd = open(ordered_blobs_path.c_str(), O_RDONLY, 0);
  ScopedFdCloser blobs_fd_closer(&blobs_fd);
  TEST_AND_RETURN_FALSE(blobs_fd >= 0);
  for (;;) {
    char buf[kBlockSize];
    ssize_t rc = read(blobs_fd, buf, sizeof(buf));
    if (0 == rc) {
      // EOF
      break;
    }
    TEST_AND_RETURN_FALSE_ERRNO(rc > 0);
    TEST_AND_RETURN_FALSE(writer.Write(buf, rc) == rc);
  }

  // Write signature blob.
  if (!private_key_path.empty()) {
    LOG(INFO) << "Signing the update...";
    vector<char> signature_blob;
    TEST_AND_RETURN_FALSE(PayloadSigner::SignPayload(output_path,
                                                     private_key_path,
                                                     &signature_blob));
    TEST_AND_RETURN_FALSE(writer.Write(&signature_blob[0],
                                       signature_blob.size()) ==
                          static_cast<ssize_t>(signature_blob.size()));
  }

  LOG(INFO) << "All done. Successfully created delta file.";
  return true;
}

const char* const kBsdiffPath = "/usr/bin/bsdiff";
const char* const kBspatchPath = "/usr/bin/bspatch";
const char* const kDeltaMagic = "CrAU";

};  // namespace chromeos_update_engine
