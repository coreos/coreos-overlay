// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/delta_diff_generator.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <bzlib.h>
#include <glog/logging.h>

#include "files/scoped_file.h"
#include "strings/string_printf.h"
#include "update_engine/bzip.h"
#include "update_engine/cycle_breaker.h"
#include "update_engine/delta_metadata.h"
#include "update_engine/ext2_metadata.h"
#include "update_engine/extent_mapper.h"
#include "update_engine/extent_ranges.h"
#include "update_engine/file_writer.h"
#include "update_engine/filesystem_iterator.h"
#include "update_engine/full_update_generator.h"
#include "update_engine/graph_types.h"
#include "update_engine/graph_utils.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/payload_signer.h"
#include "update_engine/subprocess.h"
#include "update_engine/topological_sort.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

using std::make_pair;
using std::map;
using std::max;
using std::min;
using std::pair;
using std::set;
using std::string;
using std::vector;
using strings::StringPrintf;

namespace chromeos_update_engine {

typedef DeltaDiffGenerator::Block Block;
typedef map<const InstallOperation*,
            const string*> OperationNameMap;

namespace {
const size_t kBlockSize = 4096;  // bytes
const string kNonexistentPath = "";

const uint64_t kFullUpdateChunkSize = 1024 * 1024;  // bytes

static const char* kInstallOperationTypes[] = {
  "REPLACE",
  "REPLACE_BZ",
  "MOVE",
  "BSDIFF"
};

// Stores all Extents for a file into 'out'. Returns true on success.
bool GatherExtents(const string& path,
                   google::protobuf::RepeatedPtrField<Extent>* out) {
  vector<Extent> extents;
  TEST_AND_RETURN_FALSE(extent_mapper::ExtentsForFileFibmap(path, &extents));
  DeltaDiffGenerator::StoreExtents(extents, out);
  return true;
}

// For a given regular file which must exist at new_root + path, and
// may exist at old_root + path, creates a new InstallOperation and
// adds it to the graph. Also, populates the |blocks| array as
// necessary, if |blocks| is non-NULL.  Also, writes the data
// necessary to send the file down to the client into data_fd, which
// has length *data_file_size. *data_file_size is updated
// appropriately. If |existing_vertex| is no kInvalidIndex, use that
// rather than allocating a new vertex. Returns true on success.
bool DeltaReadFile(Graph* graph,
                   Vertex::Index existing_vertex,
                   vector<Block>* blocks,
                   const string& old_root,
                   const string& new_root,
                   const string& path,  // within new_root
                   int data_fd,
                   off_t* data_file_size) {
  vector<char> data;
  InstallOperation operation;

  string old_path = (old_root == kNonexistentPath) ? kNonexistentPath :
      old_root + path;

  // If bsdiff breaks again, blacklist the problem file by using:
  //   bsdiff_allowed = (path != "/foo/bar")
  //
  // TODO(dgarrett): chromium-os:15274 connect this test to the command line.
  bool bsdiff_allowed = true;

  if (!bsdiff_allowed)
    LOG(INFO) << "bsdiff blacklisting: " << path;

  TEST_AND_RETURN_FALSE(DeltaDiffGenerator::ReadFileToDiff(old_path,
                                                           new_root + path,
                                                           bsdiff_allowed,
                                                           &data,
                                                           &operation,
                                                           true));

  // Write the data
  if (operation.type() != InstallOperation_Type_MOVE) {
    operation.set_data_offset(*data_file_size);
    operation.set_data_length(data.size());
  }

  TEST_AND_RETURN_FALSE(utils::WriteAll(data_fd, &data[0], data.size()));
  *data_file_size += data.size();

  // Now, insert into graph and blocks vector
  Vertex::Index vertex = existing_vertex;
  if (vertex == Vertex::kInvalidIndex) {
    graph->resize(graph->size() + 1);
    vertex = graph->size() - 1;
  }
  (*graph)[vertex].op = operation;
  CHECK((*graph)[vertex].op.has_type());
  (*graph)[vertex].file_name = path;

  if (blocks)
    TEST_AND_RETURN_FALSE(DeltaDiffGenerator::AddInstallOpToBlocksVector(
        (*graph)[vertex].op,
        *graph,
        vertex,
        blocks));
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
  set<ino_t> visited_src_inodes;
  for (FilesystemIterator fs_iter(new_root,
                                  set<string>{"/lost+found"});
       !fs_iter.IsEnd(); fs_iter.Increment()) {
    // We never diff symlinks (here, we check that dst file is not a symlink).
    if (!S_ISREG(fs_iter.GetStat().st_mode))
      continue;

    // Make sure we visit each inode only once.
    if (visited_inodes.count(fs_iter.GetStat().st_ino))
      continue;
    visited_inodes.insert(fs_iter.GetStat().st_ino);
    if (fs_iter.GetStat().st_size == 0)
      continue;

    LOG(INFO) << "Encoding file " << fs_iter.GetPartialPath();

    // We can't visit each dst image inode more than once, as that would
    // duplicate work. Here, we avoid visiting each source image inode
    // more than once. Technically, we could have multiple operations
    // that read the same blocks from the source image for diffing, but
    // we choose not to to avoid complexity. Eventually we will move away
    // from using a graph/cycle detection/etc to generate diffs, and at that
    // time, it will be easy (non-complex) to have many operations read
    // from the same source blocks. At that time, this code can die. -adlr
    bool should_diff_from_source = false;
    string src_path = old_root + fs_iter.GetPartialPath();
    struct stat src_stbuf;
    // We never diff symlinks (here, we check that src file is not a symlink).
    if (0 == lstat(src_path.c_str(), &src_stbuf) &&
        S_ISREG(src_stbuf.st_mode)) {
      should_diff_from_source = !visited_src_inodes.count(src_stbuf.st_ino);
      visited_src_inodes.insert(src_stbuf.st_ino);
    }

    TEST_AND_RETURN_FALSE(DeltaReadFile(graph,
                                        Vertex::kInvalidIndex,
                                        blocks,
                                        (should_diff_from_source ?
                                         old_root :
                                         kNonexistentPath),
                                        new_root,
                                        fs_iter.GetPartialPath(),
                                        data_fd,
                                        data_file_size));
  }
  return true;
}

// This class allocates non-existent temp blocks, starting from
// kTempBlockStart. Other code is responsible for converting these
// temp blocks into real blocks, as the client can't read or write to
// these blocks.
class DummyExtentAllocator {
 public:
  explicit DummyExtentAllocator()
      : next_block_(kTempBlockStart) {}
  vector<Extent> Allocate(const uint64_t block_count) {
    vector<Extent> ret(1);
    ret[0].set_start_block(next_block_);
    ret[0].set_num_blocks(block_count);
    next_block_ += block_count;
    return ret;
  }
 private:
  uint64_t next_block_;
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
                         Vertex* vertex) {
  vertex->file_name = "<fs-non-file-data>";

  InstallOperation* out_op = &vertex->op;
  int image_fd = open(image_path.c_str(), O_RDONLY, 000);
  TEST_AND_RETURN_FALSE_ERRNO(image_fd >= 0);
  files::ScopedFD image_fd_closer(image_fd);

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
    if (blocks[i].reader != Vertex::kInvalidIndex) {
      graph_utils::AddReadBeforeDep(vertex, blocks[i].reader, i);
    }
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
  for (const Extent& extent : extents) {
    vector<Block>::size_type blocks_read = 0;
    float printed_progress = -1;
    while (blocks_read < extent.num_blocks()) {
      const int copy_block_cnt =
          min(buf.size() / kBlockSize,
              static_cast<vector<char>::size_type>(
                  extent.num_blocks() - blocks_read));
      ssize_t rc = pread(image_fd,
                         &buf[0],
                         copy_block_cnt * kBlockSize,
                         (extent.start_block() + blocks_read) * kBlockSize);
      TEST_AND_RETURN_FALSE_ERRNO(rc >= 0);
      TEST_AND_RETURN_FALSE(static_cast<size_t>(rc) ==
                            copy_block_cnt * kBlockSize);
      BZ2_bzWrite(&err, bz_file, &buf[0], copy_block_cnt * kBlockSize);
      TEST_AND_RETURN_FALSE(err == BZ_OK);
      blocks_read += copy_block_cnt;
      blocks_copied_count += copy_block_cnt;
      float current_progress =
          static_cast<float>(blocks_copied_count) / block_count;
      if (printed_progress + 0.1 < current_progress ||
          blocks_copied_count == block_count) {
        LOG(INFO) << "progress: " << current_progress;
        printed_progress = current_progress;
      }
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
  out_op->set_type(InstallOperation_Type_REPLACE_BZ);
  out_op->set_data_offset(*blobs_length);
  out_op->set_data_length(compressed_data.size());
  LOG(INFO) << "fs non-data blocks compressed take up "
            << compressed_data.size();
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
  TEST_AND_RETURN_FALSE(writer->Write(&value_be, sizeof(value_be)));
  return true;
}

// Adds each operation from |graph| to |out_manifest| in the order specified by
// |order| while building |out_op_name_map| with operation to name
// mappings. Filters out no-op operations.
void InstallOperationsToManifest(
    const Graph& graph,
    const vector<Vertex::Index>& order,
    DeltaArchiveManifest* out_manifest,
    OperationNameMap* out_op_name_map) {
  for (Vertex::Index vertex_index : order) {
    const Vertex& vertex = graph[vertex_index];
    const InstallOperation& add_op = vertex.op;
    if (DeltaDiffGenerator::IsNoopOperation(add_op)) {
      continue;
    }
    InstallOperation* op =
        out_manifest->add_partition_operations();
    *op = add_op;
    (*out_op_name_map)[op] = &vertex.file_name;
  }
}

// Adds all |kernel_ops| to |manifest|. Filters out no-op operations.
// Computes hash for old and new kernel images.
bool KernelProcedureToManifest(
    const string& old_kernel_path,
    const string& new_kernel_path,
    const vector<InstallOperation>& kernel_ops,
    DeltaArchiveManifest* manifest) {
  DCHECK(!kernel_ops.empty());
  DCHECK(!new_kernel_path.empty());

  InstallProcedure* proc = manifest->add_procedures();
  proc->set_type(InstallProcedure_Type_KERNEL);
  for (const InstallOperation& add_op : kernel_ops) {
    if (DeltaDiffGenerator::IsNoopOperation(add_op)) {
      continue;
    }
    InstallOperation* op = proc->add_operations();
    *op = add_op;
  }

  if (!old_kernel_path.empty()) {
    TEST_AND_RETURN_FALSE(
        DeltaDiffGenerator::InitializeInfo(old_kernel_path,
                                           proc->mutable_old_info()));
  }
  TEST_AND_RETURN_FALSE(
      DeltaDiffGenerator::InitializeInfo(new_kernel_path,
                                         proc->mutable_new_info()));
  return true;
}

// Create dummy operations to cover data required by extra InstallProcedures
// in the noop_operations list for compatibility with old versions that did
// not support the new procedures.
void ProceduresToNoops(DeltaArchiveManifest* manifest) {
  for (const InstallProcedure& proc : manifest->procedures()) {
    for (const InstallOperation& op : proc.operations()) {
      InstallOperation* noop = manifest->add_noop_operations();
      noop->set_type(InstallOperation_Type_REPLACE);
      noop->set_data_offset(op.data_offset());
      noop->set_data_length(op.data_length());
      // Tell the dummy op to write this data to a big sparse hole
      Extent* extent = noop->add_dst_extents();
      extent->set_start_block(kSparseHole);
      extent->set_num_blocks((op.data_length() + kBlockSize - 1) / kBlockSize);
    }
  }
}

void CheckGraph(const Graph& graph) {
  for (const Vertex& v : graph) {
    CHECK(v.op.has_type());
  }
}

// Delta compresses a kernel partition |new_kernel| with knowledge of the
// old kernel partition |old_kernel|. If |old_kernel| is an empty string,
// generates a full update of the partition.
bool DeltaCompressKernel(
    const string& old_kernel,
    const string& new_kernel,
    vector<InstallOperation>* ops,
    int blobs_fd,
    off_t* blobs_length) {
  LOG(INFO) << "Delta compressing kernel partition...";
  LOG_IF(INFO, old_kernel.empty()) << "Generating full kernel update...";

  // Add a new install operation
  ops->resize(1);
  InstallOperation* op = &(*ops)[0];

  vector<char> data;
  TEST_AND_RETURN_FALSE(
      DeltaDiffGenerator::ReadFileToDiff(old_kernel,
                                         new_kernel,
                                         true, // bsdiff_allowed
                                         &data,
                                         op,
                                         false));

  // Write the data
  if (op->type() != InstallOperation_Type_MOVE) {
    op->set_data_offset(*blobs_length);
    op->set_data_length(data.size());
  }

  TEST_AND_RETURN_FALSE(utils::WriteAll(blobs_fd, &data[0], data.size()));
  *blobs_length += data.size();

  LOG(INFO) << "Done delta compressing kernel partition: "
            << kInstallOperationTypes[op->type()];
  return true;
}

struct DeltaObject {
  DeltaObject(const string& in_name, const int in_type, const off_t in_size)
      : name(in_name),
        type(in_type),
        size(in_size) {}
  bool operator <(const DeltaObject& object) const {
    return (size != object.size) ? (size < object.size) : (name < object.name);
  }
  string name;
  int type;
  off_t size;
};

void ReportPayloadUsage(const DeltaArchiveManifest& manifest,
                        const int64_t manifest_metadata_size,
                        const OperationNameMap& op_name_map) {
  vector<DeltaObject> objects;
  off_t total_size = 0;

  // Rootfs install operations.
  for (int i = 0; i < manifest.partition_operations_size(); ++i) {
    const InstallOperation& op =
        manifest.partition_operations(i);
    objects.push_back(DeltaObject(*op_name_map.find(&op)->second,
                                  op.type(),
                                  op.data_length()));
    total_size += op.data_length();
  }

  // Dummy install operations for compatibility with older clients.
  for (int i = 0; i < manifest.noop_operations_size(); ++i) {
    const InstallOperation& op =
        manifest.noop_operations(i);
    objects.push_back(DeltaObject(StringPrintf("<noop-operation-%d>", i),
                                  op.type(),
                                  op.data_length()));
    total_size += op.data_length();
  }

  objects.push_back(DeltaObject("<manifest-metadata>",
                                -1,
                                manifest_metadata_size));
  total_size += manifest_metadata_size;

  std::sort(objects.begin(), objects.end());

  static const char kFormatString[] = "%6.2f%% %10jd %-10s %s\n";
  for (const DeltaObject& object : objects) {
    fprintf(stderr, kFormatString,
            object.size * 100.0 / total_size,
            static_cast<intmax_t>(object.size),
            object.type >= 0 ? kInstallOperationTypes[object.type] : "-",
            object.name.c_str());
  }
  fprintf(stderr, kFormatString,
          100.0, static_cast<intmax_t>(total_size), "", "<total>");
}

}  // namespace {}

bool DeltaDiffGenerator::ReadFileToDiff(
    const string& old_filename,
    const string& new_filename,
    bool bsdiff_allowed,
    vector<char>* out_data,
    InstallOperation* out_op,
    bool gather_extents) {
  // Read new data in
  vector<char> new_data;
  TEST_AND_RETURN_FALSE(utils::ReadFile(new_filename, &new_data));

  TEST_AND_RETURN_FALSE(!new_data.empty());

  vector<char> new_data_bz;
  TEST_AND_RETURN_FALSE(BzipCompress(new_data, &new_data_bz));
  CHECK(!new_data_bz.empty());

  vector<char> data;  // Data blob that will be written to delta file.

  InstallOperation operation;
  size_t current_best_size = 0;
  if (new_data.size() <= new_data_bz.size()) {
    operation.set_type(InstallOperation_Type_REPLACE);
    current_best_size = new_data.size();
    data = new_data;
  } else {
    operation.set_type(InstallOperation_Type_REPLACE_BZ);
    current_best_size = new_data_bz.size();
    data = new_data_bz;
  }

  // Do we have an original file to consider?
  struct stat old_stbuf;
  bool original = !old_filename.empty();
  if (original && 0 != stat(old_filename.c_str(), &old_stbuf)) {
    // If stat-ing the old file fails, it should be because it doesn't exist.
    TEST_AND_RETURN_FALSE(errno == ENOTDIR || errno == ENOENT);
    original = false;
  }

  if (original) {
    // Read old data
    vector<char> old_data;
    TEST_AND_RETURN_FALSE(utils::ReadFile(old_filename, &old_data));
    if (old_data == new_data) {
      // No change in data.
      operation.set_type(InstallOperation_Type_MOVE);
      current_best_size = 0;
      data.clear();
    } else if (bsdiff_allowed) {
      // If the source file is considered bsdiff safe (no bsdiff bugs
      // triggered), see if BSDIFF encoding is smaller.
      vector<char> bsdiff_delta;
      TEST_AND_RETURN_FALSE(
          BsdiffFiles(old_filename, new_filename, &bsdiff_delta));
      CHECK_GT(bsdiff_delta.size(), static_cast<vector<char>::size_type>(0));
      if (bsdiff_delta.size() < current_best_size) {
        operation.set_type(InstallOperation_Type_BSDIFF);
        current_best_size = bsdiff_delta.size();
        data = bsdiff_delta;
      }
    }
  }

  // Set parameters of the operations
  CHECK_EQ(data.size(), current_best_size);

  if (operation.type() == InstallOperation_Type_MOVE ||
      operation.type() == InstallOperation_Type_BSDIFF) {
    if (gather_extents) {
      TEST_AND_RETURN_FALSE(
          GatherExtents(old_filename, operation.mutable_src_extents()));
    } else {
      Extent* src_extent = operation.add_src_extents();
      src_extent->set_start_block(0);
      src_extent->set_num_blocks(
          (old_stbuf.st_size + kBlockSize - 1) / kBlockSize);
    }
    operation.set_src_length(old_stbuf.st_size);
  }

  if (gather_extents) {
    TEST_AND_RETURN_FALSE(
        GatherExtents(new_filename, operation.mutable_dst_extents()));
  } else {
    Extent* dst_extent = operation.add_dst_extents();
    dst_extent->set_start_block(0);
    dst_extent->set_num_blocks((new_data.size() + kBlockSize - 1) / kBlockSize);
  }
  operation.set_dst_length(new_data.size());

  out_data->swap(data);
  *out_op = operation;

  return true;
}

bool DeltaDiffGenerator::InitializeInfo(const string& path, InstallInfo* info) {
  off_t size = 0;
  TEST_AND_RETURN_FALSE(utils::GetDeviceSize(path, &size));
  info->set_size(size);
  OmahaHashCalculator hasher;
  TEST_AND_RETURN_FALSE(hasher.UpdateFile(path, size) == size);
  TEST_AND_RETURN_FALSE(hasher.Finalize());
  const vector<char>& hash = hasher.raw_hash();
  info->set_hash(hash.data(), hash.size());
  LOG(INFO) << path << ": size=" << size << " hash=" << hasher.hash();
  return true;
}

bool InitializePartitionInfos(const string& old_rootfs,
                              const string& new_rootfs,
                              DeltaArchiveManifest& manifest) {
  if (!old_rootfs.empty()) {
    TEST_AND_RETURN_FALSE(DeltaDiffGenerator::InitializeInfo(
        old_rootfs,
        manifest.mutable_old_partition_info()));
  }
  TEST_AND_RETURN_FALSE(DeltaDiffGenerator::InitializeInfo(
      new_rootfs,
      manifest.mutable_new_partition_info()));
  return true;
}

namespace {

// Takes a collection (vector or RepeatedPtrField) of Extent and
// returns a vector of the blocks referenced, in order.
template<typename T>
vector<uint64_t> ExpandExtents(const T& extents) {
  vector<uint64_t> ret;
  for (size_t i = 0, e = static_cast<size_t>(extents.size()); i != e; ++i) {
    const Extent extent = graph_utils::GetElement(extents, i);
    if (extent.start_block() == kSparseHole) {
      ret.resize(ret.size() + extent.num_blocks(), kSparseHole);
    } else {
      for (uint64_t block = extent.start_block();
           block < (extent.start_block() + extent.num_blocks()); block++) {
        ret.push_back(block);
      }
    }
  }
  return ret;
}

// Takes a vector of blocks and returns an equivalent vector of Extent
// objects.
vector<Extent> CompressExtents(const vector<uint64_t>& blocks) {
  vector<Extent> new_extents;
  for (uint64_t block : blocks) {
    graph_utils::AppendBlockToExtents(&new_extents, block);
  }
  return new_extents;
}

}  // namespace {}

void DeltaDiffGenerator::SubstituteBlocks(
    Vertex* vertex,
    const vector<Extent>& remove_extents,
    const vector<Extent>& replace_extents) {
  // First, expand out the blocks that op reads from
  vector<uint64_t> read_blocks = ExpandExtents(vertex->op.src_extents());
  {
    // Expand remove_extents and replace_extents
    vector<uint64_t> remove_extents_expanded =
        ExpandExtents(remove_extents);
    vector<uint64_t> replace_extents_expanded =
        ExpandExtents(replace_extents);
    CHECK_EQ(remove_extents_expanded.size(), replace_extents_expanded.size());
    map<uint64_t, uint64_t> conversion;
    for (vector<uint64_t>::size_type i = 0;
         i < replace_extents_expanded.size(); i++) {
      conversion[remove_extents_expanded[i]] = replace_extents_expanded[i];
    }
    utils::ApplyMap(&read_blocks, conversion);
    for (auto& edge_prop_pair : vertex->out_edges) {
      vector<uint64_t> write_before_deps_expanded =
          ExpandExtents(edge_prop_pair.second.write_extents);
      utils::ApplyMap(&write_before_deps_expanded, conversion);
      edge_prop_pair.second.write_extents =
          CompressExtents(write_before_deps_expanded);
    }
  }
  // Convert read_blocks back to extents
  vertex->op.clear_src_extents();
  vector<Extent> new_extents = CompressExtents(read_blocks);
  DeltaDiffGenerator::StoreExtents(new_extents,
                                   vertex->op.mutable_src_extents());
}

bool DeltaDiffGenerator::CutEdges(Graph* graph,
                                  const set<Edge>& edges,
                                  vector<CutEdgeVertexes>* out_cuts) {
  DummyExtentAllocator scratch_allocator;
  vector<CutEdgeVertexes> cuts;
  cuts.reserve(edges.size());

  uint64_t scratch_blocks_used = 0;
  for (const Edge& edge : edges) {
    cuts.resize(cuts.size() + 1);
    vector<Extent> old_extents =
        (*graph)[edge.first].out_edges[edge.second].extents;
    // Choose some scratch space
    scratch_blocks_used += graph_utils::EdgeWeight(*graph, edge);
    cuts.back().tmp_extents =
        scratch_allocator.Allocate(graph_utils::EdgeWeight(*graph, edge));
    // create vertex to copy original->scratch
    cuts.back().new_vertex = graph->size();
    graph->resize(graph->size() + 1);
    cuts.back().old_src = edge.first;
    cuts.back().old_dst = edge.second;

    EdgeProperties& cut_edge_properties =
        (*graph)[edge.first].out_edges.find(edge.second)->second;

    // This should never happen, as we should only be cutting edges between
    // real file nodes, and write-before relationships are created from
    // a real file node to a temp copy node:
    CHECK(cut_edge_properties.write_extents.empty())
        << "Can't cut edge that has write-before relationship.";

    // make node depend on the copy operation
    (*graph)[edge.first].out_edges.insert(make_pair(graph->size() - 1,
                                                   cut_edge_properties));

    // Set src/dst extents and other proto variables for copy operation
    graph->back().op.set_type(InstallOperation_Type_MOVE);
    DeltaDiffGenerator::StoreExtents(
        cut_edge_properties.extents,
        graph->back().op.mutable_src_extents());
    DeltaDiffGenerator::StoreExtents(cuts.back().tmp_extents,
                                     graph->back().op.mutable_dst_extents());
    graph->back().op.set_src_length(
        graph_utils::EdgeWeight(*graph, edge) * kBlockSize);
    graph->back().op.set_dst_length(graph->back().op.src_length());

    // make the dest node read from the scratch space
    DeltaDiffGenerator::SubstituteBlocks(
        &((*graph)[edge.second]),
        (*graph)[edge.first].out_edges[edge.second].extents,
        cuts.back().tmp_extents);

    // delete the old edge
    CHECK_EQ(static_cast<Graph::size_type>(1),
             (*graph)[edge.first].out_edges.erase(edge.second));

    // Add an edge from dst to copy operation
    EdgeProperties write_before_edge_properties;
    write_before_edge_properties.write_extents = cuts.back().tmp_extents;
    (*graph)[edge.second].out_edges.insert(
        make_pair(graph->size() - 1, write_before_edge_properties));
  }
  out_cuts->swap(cuts);
  return true;
}

// Stores all Extents in 'extents' into 'out'.
void DeltaDiffGenerator::StoreExtents(
    const vector<Extent>& extents,
    google::protobuf::RepeatedPtrField<Extent>* out) {
  for (const Extent& extent : extents) {
    Extent* new_extent = out->Add();
    *new_extent = extent;
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

namespace {

class SortCutsByTopoOrderLess {
 public:
  SortCutsByTopoOrderLess(vector<vector<Vertex::Index>::size_type>& table)
      : table_(table) {}
  bool operator()(const CutEdgeVertexes& a, const CutEdgeVertexes& b) {
    return table_[a.old_dst] < table_[b.old_dst];
  }
 private:
  vector<vector<Vertex::Index>::size_type>& table_;
};

}  // namespace {}

void DeltaDiffGenerator::GenerateReverseTopoOrderMap(
    vector<Vertex::Index>& op_indexes,
    vector<vector<Vertex::Index>::size_type>* reverse_op_indexes) {
  vector<vector<Vertex::Index>::size_type> table(op_indexes.size());
  for (vector<Vertex::Index>::size_type i = 0, e = op_indexes.size();
       i != e; ++i) {
    Vertex::Index node = op_indexes[i];
    if (table.size() < (node + 1)) {
      table.resize(node + 1);
    }
    table[node] = i;
  }
  reverse_op_indexes->swap(table);
}

void DeltaDiffGenerator::SortCutsByTopoOrder(vector<Vertex::Index>& op_indexes,
                                             vector<CutEdgeVertexes>* cuts) {
  // first, make a reverse lookup table.
  vector<vector<Vertex::Index>::size_type> table;
  GenerateReverseTopoOrderMap(op_indexes, &table);
  SortCutsByTopoOrderLess less(table);
  sort(cuts->begin(), cuts->end(), less);
}

void DeltaDiffGenerator::MoveFullOpsToBack(Graph* graph,
                                           vector<Vertex::Index>* op_indexes) {
  vector<Vertex::Index> ret;
  vector<Vertex::Index> full_ops;
  ret.reserve(op_indexes->size());
  for (vector<Vertex::Index>::size_type i = 0, e = op_indexes->size(); i != e;
       ++i) {
    InstallOperation_Type type =
        (*graph)[(*op_indexes)[i]].op.type();
    if (type == InstallOperation_Type_REPLACE ||
        type == InstallOperation_Type_REPLACE_BZ) {
      full_ops.push_back((*op_indexes)[i]);
    } else {
      ret.push_back((*op_indexes)[i]);
    }
  }
  LOG(INFO) << "Stats: " << full_ops.size() << " full ops out of "
            << (full_ops.size() + ret.size()) << " total ops.";
  ret.insert(ret.end(), full_ops.begin(), full_ops.end());
  op_indexes->swap(ret);
}

namespace {

template<typename T>
bool TempBlocksExistInExtents(const T& extents) {
  for (int i = 0, e = extents.size(); i < e; ++i) {
    Extent extent = graph_utils::GetElement(extents, i);
    uint64_t start = extent.start_block();
    uint64_t num = extent.num_blocks();
    if (start == kSparseHole)
      continue;
    if (start >= kTempBlockStart ||
        (start + num) >= kTempBlockStart) {
      LOG(ERROR) << "temp block!";
      LOG(ERROR) << "start: " << start << ", num: " << num;
      LOG(ERROR) << "kTempBlockStart: " << kTempBlockStart;
      LOG(ERROR) << "returning true";
      return true;
    }
    // check for wrap-around, which would be a bug:
    CHECK(start <= (start + num));
  }
  return false;
}

// Convertes the cuts, which must all have the same |old_dst| member,
// to full. It does this by converting the |old_dst| to REPLACE or
// REPLACE_BZ, dropping all incoming edges to |old_dst|, and marking
// all temp nodes invalid.
bool ConvertCutsToFull(
    Graph* graph,
    const string& new_root,
    int data_fd,
    off_t* data_file_size,
    vector<Vertex::Index>* op_indexes,
    vector<vector<Vertex::Index>::size_type>* reverse_op_indexes,
    const vector<CutEdgeVertexes>& cuts) {
  CHECK(!cuts.empty());
  set<Vertex::Index> deleted_nodes;
  for (const CutEdgeVertexes& cut : cuts) {
    TEST_AND_RETURN_FALSE(DeltaDiffGenerator::ConvertCutToFullOp(
        graph,
        cut,
        new_root,
        data_fd,
        data_file_size));
    deleted_nodes.insert(cut.new_vertex);
  }
  deleted_nodes.insert(cuts[0].old_dst);

  vector<Vertex::Index> new_op_indexes;
  new_op_indexes.reserve(op_indexes->size());
  for (Vertex::Index vertex_index : *op_indexes) {
    if (deleted_nodes.count(vertex_index))
      continue;
    new_op_indexes.push_back(vertex_index);
  }
  new_op_indexes.push_back(cuts[0].old_dst);
  op_indexes->swap(new_op_indexes);
  DeltaDiffGenerator::GenerateReverseTopoOrderMap(*op_indexes,
                                                  reverse_op_indexes);
  return true;
}

// Tries to assign temp blocks for a collection of cuts, all of which share
// the same old_dst member. If temp blocks can't be found, old_dst will be
// converted to a REPLACE or REPLACE_BZ operation. Returns true on success,
// which can happen even if blocks are converted to full. Returns false
// on exceptional error cases.
bool AssignBlockForAdjoiningCuts(
    Graph* graph,
    const string& new_root,
    int data_fd,
    off_t* data_file_size,
    vector<Vertex::Index>* op_indexes,
    vector<vector<Vertex::Index>::size_type>* reverse_op_indexes,
    const vector<CutEdgeVertexes>& cuts) {
  CHECK(!cuts.empty());
  const Vertex::Index old_dst = cuts[0].old_dst;
  // Calculate # of blocks needed
  uint64_t blocks_needed = 0;
  vector<uint64_t> cuts_blocks_needed(cuts.size());
  for (vector<CutEdgeVertexes>::size_type i = 0; i < cuts.size(); ++i) {
    uint64_t cut_blocks_needed = 0;
    for (const Extent& extent : cuts[i].tmp_extents) {
      cut_blocks_needed += extent.num_blocks();
    }
    blocks_needed += cut_blocks_needed;
    cuts_blocks_needed[i] = cut_blocks_needed;
  }

  // Find enough blocks
  ExtentRanges scratch_ranges;
  // Each block that's supplying temp blocks and the corresponding blocks:
  typedef vector<pair<Vertex::Index, ExtentRanges> > SupplierVector;
  SupplierVector block_suppliers;
  uint64_t scratch_blocks_found = 0;
  for (vector<Vertex::Index>::size_type i = (*reverse_op_indexes)[old_dst] + 1,
           e = op_indexes->size(); i < e; ++i) {
    Vertex::Index test_node = (*op_indexes)[i];
    if (!(*graph)[test_node].valid)
      continue;
    // See if this node has sufficient blocks
    ExtentRanges ranges;
    ranges.AddRepeatedExtents((*graph)[test_node].op.dst_extents());
    ranges.SubtractExtent(ExtentForRange(
        kTempBlockStart, kSparseHole - kTempBlockStart));
    ranges.SubtractRepeatedExtents((*graph)[test_node].op.src_extents());
    // For now, for simplicity, subtract out all blocks in read-before
    // dependencies.
    for (Vertex::EdgeMap::const_iterator edge_i =
             (*graph)[test_node].out_edges.begin(),
             edge_e = (*graph)[test_node].out_edges.end();
         edge_i != edge_e; ++edge_i) {
      ranges.SubtractExtents(edge_i->second.extents);
    }
    if (ranges.blocks() == 0)
      continue;

    if (ranges.blocks() + scratch_blocks_found > blocks_needed) {
      // trim down ranges
      vector<Extent> new_ranges = ranges.GetExtentsForBlockCount(
          blocks_needed - scratch_blocks_found);
      ranges = ExtentRanges();
      ranges.AddExtents(new_ranges);
    }
    scratch_ranges.AddRanges(ranges);
    block_suppliers.push_back(make_pair(test_node, ranges));
    scratch_blocks_found += ranges.blocks();
    if (scratch_ranges.blocks() >= blocks_needed)
      break;
  }
  if (scratch_ranges.blocks() < blocks_needed) {
    LOG(INFO) << "Unable to find sufficient scratch";
    TEST_AND_RETURN_FALSE(ConvertCutsToFull(graph,
                                            new_root,
                                            data_fd,
                                            data_file_size,
                                            op_indexes,
                                            reverse_op_indexes,
                                            cuts));
    return true;
  }
  // Use the scratch we found
  TEST_AND_RETURN_FALSE(scratch_ranges.blocks() == scratch_blocks_found);

  // Make all the suppliers depend on this node
  for (const auto& index_range_pair : block_suppliers) {
    graph_utils::AddReadBeforeDepExtents(
        &(*graph)[index_range_pair.first],
        old_dst,
        index_range_pair.second.GetExtentsForBlockCount(
            index_range_pair.second.blocks()));
  }

  // Replace temp blocks in each cut
  for (vector<CutEdgeVertexes>::size_type i = 0; i < cuts.size(); ++i) {
    const CutEdgeVertexes& cut = cuts[i];
    vector<Extent> real_extents =
        scratch_ranges.GetExtentsForBlockCount(cuts_blocks_needed[i]);
    scratch_ranges.SubtractExtents(real_extents);

    // Fix the old dest node w/ the real blocks
    DeltaDiffGenerator::SubstituteBlocks(&(*graph)[old_dst],
                                         cut.tmp_extents,
                                         real_extents);

    // Fix the new node w/ the real blocks. Since the new node is just a
    // copy operation, we can replace all the dest extents w/ the real
    // blocks.
    InstallOperation& op = (*graph)[cut.new_vertex].op;
    op.clear_dst_extents();
    DeltaDiffGenerator::StoreExtents(real_extents, op.mutable_dst_extents());
  }
  return true;
}

}  // namespace {}

// Returns true if |op| is a no-op operation that doesn't do any useful work
// (e.g., a move operation that copies blocks onto themselves).
bool DeltaDiffGenerator::IsNoopOperation(
    const InstallOperation& op) {
  return (op.type() == InstallOperation_Type_MOVE &&
          ExpandExtents(op.src_extents()) == ExpandExtents(op.dst_extents()));
}

bool DeltaDiffGenerator::AssignTempBlocks(
    Graph* graph,
    const string& new_root,
    int data_fd,
    off_t* data_file_size,
    vector<Vertex::Index>* op_indexes,
    vector<vector<Vertex::Index>::size_type>* reverse_op_indexes,
    const vector<CutEdgeVertexes>& cuts) {
  CHECK(!cuts.empty());

  // group of cuts w/ the same old_dst:
  vector<CutEdgeVertexes> cuts_group;

  for (vector<CutEdgeVertexes>::size_type i = cuts.size() - 1, e = 0;
       true ; --i) {
    LOG(INFO) << "Fixing temp blocks in cut " << i
              << ": old dst: " << cuts[i].old_dst << " new vertex: "
              << cuts[i].new_vertex << " path: "
              << (*graph)[cuts[i].old_dst].file_name;

    if (cuts_group.empty() || (cuts_group[0].old_dst == cuts[i].old_dst)) {
      cuts_group.push_back(cuts[i]);
    } else {
      CHECK(!cuts_group.empty());
      TEST_AND_RETURN_FALSE(AssignBlockForAdjoiningCuts(graph,
                                                        new_root,
                                                        data_fd,
                                                        data_file_size,
                                                        op_indexes,
                                                        reverse_op_indexes,
                                                        cuts_group));
      cuts_group.clear();
      cuts_group.push_back(cuts[i]);
    }

    if (i == e) {
      // break out of for() loop
      break;
    }
  }
  CHECK(!cuts_group.empty());
  TEST_AND_RETURN_FALSE(AssignBlockForAdjoiningCuts(graph,
                                                    new_root,
                                                    data_fd,
                                                    data_file_size,
                                                    op_indexes,
                                                    reverse_op_indexes,
                                                    cuts_group));
  return true;
}

bool DeltaDiffGenerator::NoTempBlocksRemain(const Graph& graph) {
  size_t idx = 0;
  for (Graph::const_iterator it = graph.begin(), e = graph.end(); it != e;
       ++it, ++idx) {
    if (!it->valid)
      continue;
    const InstallOperation& op = it->op;
    if (TempBlocksExistInExtents(op.dst_extents()) ||
        TempBlocksExistInExtents(op.src_extents())) {
      LOG(INFO) << "bad extents in node " << idx;
      LOG(INFO) << "so yeah";
      return false;
    }

    // Check out-edges:
    for (const auto& edge_prop_pair : it->out_edges) {
      if (TempBlocksExistInExtents(edge_prop_pair.second.extents) ||
          TempBlocksExistInExtents(edge_prop_pair.second.write_extents)) {
        LOG(INFO) << "bad out edge in node " << idx;
        LOG(INFO) << "so yeah";
        return false;
      }
    }
  }
  return true;
}

static bool ReorderProcedureDataBlobs(
    google::protobuf::RepeatedPtrField<InstallOperation>* ops,
    int in_fd,
    DirectFileWriter* writer,
    uint64_t* written) {
  for (InstallOperation& op : *ops) {
    if (!op.has_data_offset())
      continue;

    CHECK(op.has_data_length());
    vector<char> buf(op.data_length());
    ssize_t rc = pread(in_fd, &buf[0], buf.size(), op.data_offset());
    TEST_AND_RETURN_FALSE(rc == static_cast<ssize_t>(buf.size()));

    // Add the hash of the data blobs for this operation
    TEST_AND_RETURN_FALSE(DeltaDiffGenerator::AddOperationHash(&op, buf));

    op.set_data_offset(*written);
    TEST_AND_RETURN_FALSE(writer->Write(&buf[0], buf.size()));
    *written += buf.size();
  }

  return true;
}

bool DeltaDiffGenerator::ReorderDataBlobs(
    DeltaArchiveManifest* manifest,
    const std::string& data_blobs_path,
    const std::string& new_data_blobs_path) {
  int in_fd = open(data_blobs_path.c_str(), O_RDONLY, 0);
  TEST_AND_RETURN_FALSE_ERRNO(in_fd >= 0);
  files::ScopedFD in_fd_closer(in_fd);

  DirectFileWriter writer(new_data_blobs_path.c_str());
  TEST_AND_RETURN_FALSE_ERRNO(writer.Open() == 0);
  ScopedFileWriterCloser writer_closer(&writer);
  uint64_t written = 0;

  TEST_AND_RETURN_FALSE(ReorderProcedureDataBlobs(
      manifest->mutable_partition_operations(),
      in_fd,
      &writer,
      &written));

  for (InstallProcedure& proc : *manifest->mutable_procedures()) {
    TEST_AND_RETURN_FALSE(ReorderProcedureDataBlobs(
        proc.mutable_operations(),
        in_fd,
        &writer,
        &written));
  }

  return true;
}

bool DeltaDiffGenerator::AddOperationHash(
    InstallOperation* op,
    const vector<char>& buf) {
  OmahaHashCalculator hasher;

  TEST_AND_RETURN_FALSE(hasher.Update(&buf[0], buf.size()));
  TEST_AND_RETURN_FALSE(hasher.Finalize());

  const vector<char>& hash = hasher.raw_hash();
  op->set_data_sha256_hash(hash.data(), hash.size());
  return true;
}

bool DeltaDiffGenerator::ConvertCutToFullOp(Graph* graph,
                                            const CutEdgeVertexes& cut,
                                            const string& new_root,
                                            int data_fd,
                                            off_t* data_file_size) {
  // Drop all incoming edges, keep all outgoing edges

  // Keep all outgoing edges
  if ((*graph)[cut.old_dst].op.type() !=
      InstallOperation_Type_REPLACE_BZ &&
      (*graph)[cut.old_dst].op.type() !=
      InstallOperation_Type_REPLACE) {
    Vertex::EdgeMap out_edges = (*graph)[cut.old_dst].out_edges;
    graph_utils::DropWriteBeforeDeps(&out_edges);

    TEST_AND_RETURN_FALSE(DeltaReadFile(graph,
                                        cut.old_dst,
                                        NULL,
                                        kNonexistentPath,
                                        new_root,
                                        (*graph)[cut.old_dst].file_name,
                                        data_fd,
                                        data_file_size));

    (*graph)[cut.old_dst].out_edges = out_edges;

    // Right now we don't have doubly-linked edges, so we have to scan
    // the whole graph.
    graph_utils::DropIncomingEdgesTo(graph, cut.old_dst);
  }

  // Delete temp node
  (*graph)[cut.old_src].out_edges.erase(cut.new_vertex);
  CHECK((*graph)[cut.old_dst].out_edges.find(cut.new_vertex) ==
        (*graph)[cut.old_dst].out_edges.end());
  (*graph)[cut.new_vertex].valid = false;
  LOG(INFO) << "marked node invalid: " << cut.new_vertex;
  return true;
}

bool DeltaDiffGenerator::ConvertGraphToDag(Graph* graph,
                                           const string& new_root,
                                           int fd,
                                           off_t* data_file_size,
                                           vector<Vertex::Index>* final_order) {
  CycleBreaker cycle_breaker;
  LOG(INFO) << "Finding cycles...";
  set<Edge> cut_edges;
  cycle_breaker.BreakCycles(*graph, &cut_edges);
  LOG(INFO) << "done finding cycles";
  CheckGraph(*graph);

  // Calculate number of scratch blocks needed
  LOG(INFO) << "Cutting cycles...";
  vector<CutEdgeVertexes> cuts;
  TEST_AND_RETURN_FALSE(CutEdges(graph, cut_edges, &cuts));
  LOG(INFO) << "done cutting cycles";
  LOG(INFO) << "There are " << cuts.size() << " cuts.";
  CheckGraph(*graph);

  LOG(INFO) << "Creating initial topological order...";
  TopologicalSort(*graph, final_order);
  LOG(INFO) << "done with initial topo order";
  CheckGraph(*graph);

  LOG(INFO) << "Moving full ops to the back";
  MoveFullOpsToBack(graph, final_order);
  LOG(INFO) << "done moving full ops to back";

  vector<vector<Vertex::Index>::size_type> inverse_final_order;
  GenerateReverseTopoOrderMap(*final_order, &inverse_final_order);

  SortCutsByTopoOrder(*final_order, &cuts);

  if (!cuts.empty())
    TEST_AND_RETURN_FALSE(AssignTempBlocks(graph,
                                           new_root,
                                           fd,
                                           data_file_size,
                                           final_order,
                                           &inverse_final_order,
                                           cuts));
  LOG(INFO) << "Making sure all temp blocks have been allocated";
  graph_utils::DumpGraph(*graph);
  CHECK(NoTempBlocksRemain(*graph));
  LOG(INFO) << "done making sure all temp blocks are allocated";
  return true;
}

bool DeltaDiffGenerator::GenerateDeltaUpdateFile(
    const string& old_root,
    const string& old_image,
    const string& new_root,
    const string& new_image,
    const string& old_kernel,
    const string& new_kernel,
    const string& output_path,
    const string& private_key_path,
    uint64_t* metadata_size) {
  off_t new_image_size = 0, old_image_size = 0;
  TEST_AND_RETURN_FALSE(utils::GetDeviceSize(new_image, &new_image_size));
  if (new_image_size % kBlockSize != 0) {
    LOG(ERROR) << "Size of " << new_image << " (" << new_image_size
      << ") is not a multiple of " << kBlockSize;
    return false;
  }

  if (!old_image.empty()) {
    TEST_AND_RETURN_FALSE(utils::GetDeviceSize(old_image, &old_image_size));
    if (old_image_size % kBlockSize != 0) {
      LOG(ERROR) << "Size of " << old_image << " (" << old_image_size
        << ") is not a multiple of " << kBlockSize;
      return false;
    }
    if (old_image_size != new_image_size) {
      LOG(ERROR) << "Old and new images have different sizes: "
        << old_image_size << " != " << new_image_size;
      return false;
    }
  }

  if (!new_kernel.empty()) {
    // Sanity check kernel partition arg
    TEST_AND_RETURN_FALSE(utils::FileSize(new_kernel) >= 0);
  }

  vector<Block> blocks(new_image_size / kBlockSize);
  LOG(INFO) << "Invalid block index: " << Vertex::kInvalidIndex;
  LOG(INFO) << "Block count: " << blocks.size();
  for (vector<Block>::size_type i = 0; i < blocks.size(); i++) {
    CHECK(blocks[i].reader == Vertex::kInvalidIndex);
    CHECK(blocks[i].writer == Vertex::kInvalidIndex);
  }
  Graph graph;
  CheckGraph(graph);

  const string kTempFileTemplate("/tmp/CrAU_temp_data.XXXXXX");
  string temp_file_path;
  std::unique_ptr<ScopedPathUnlinker> temp_file_unlinker;
  off_t data_file_size = 0;

  LOG(INFO) << "Reading files...";

  vector<InstallOperation> kernel_ops;

  vector<Vertex::Index> final_order;
  {
    int fd;
    TEST_AND_RETURN_FALSE(
        utils::MakeTempFile(kTempFileTemplate, &temp_file_path, &fd));
    temp_file_unlinker.reset(new ScopedPathUnlinker(temp_file_path));
    TEST_AND_RETURN_FALSE(fd >= 0);
    files::ScopedFD fd_closer(fd);
    if (!old_image.empty()) {
      // Delta update

      TEST_AND_RETURN_FALSE(DeltaReadFiles(&graph,
                                           &blocks,
                                           old_root,
                                           new_root,
                                           fd,
                                           &data_file_size));
      LOG(INFO) << "done reading normal files";
      CheckGraph(graph);

      LOG(INFO) << "Starting metadata processing";
      TEST_AND_RETURN_FALSE(Ext2Metadata::DeltaReadMetadata(&graph,
                                                            &blocks,
                                                            old_image,
                                                            new_image,
                                                            fd,
                                                            &data_file_size));
      LOG(INFO) << "Done metadata processing";
      CheckGraph(graph);

      graph.resize(graph.size() + 1);
      TEST_AND_RETURN_FALSE(ReadUnwrittenBlocks(blocks,
                                                fd,
                                                &data_file_size,
                                                new_image,
                                                &graph.back()));

      if (!new_kernel.empty()) {
        // Read kernel partition
        TEST_AND_RETURN_FALSE(DeltaCompressKernel(old_kernel,
                                                  new_kernel,
                                                  &kernel_ops,
                                                  fd,
                                                  &data_file_size));
        LOG(INFO) << "done reading kernel";
      }

      CheckGraph(graph);

      LOG(INFO) << "Creating edges...";
      CreateEdges(&graph, blocks);
      LOG(INFO) << "Done creating edges";
      CheckGraph(graph);

      TEST_AND_RETURN_FALSE(ConvertGraphToDag(&graph,
                                              new_root,
                                              fd,
                                              &data_file_size,
                                              &final_order));
    } else {
      FullUpdateGenerator generator(fd, kFullUpdateChunkSize, kBlockSize);

      TEST_AND_RETURN_FALSE(generator.Partition(new_image,
                                                new_image_size,
                                                &graph,
                                                &final_order));

      if (!new_kernel.empty())
        TEST_AND_RETURN_FALSE(generator.Add(new_kernel, &kernel_ops));

      data_file_size = generator.Size();
    }
  }

  // Convert to protobuf Manifest object
  DeltaArchiveManifest manifest;
  OperationNameMap op_name_map;
  CheckGraph(graph);
  InstallOperationsToManifest(graph,
                              final_order,
                              &manifest,
                              &op_name_map);
  CheckGraph(graph);
  manifest.set_block_size(kBlockSize);

  if (!new_kernel.empty()) {
    TEST_AND_RETURN_FALSE(KernelProcedureToManifest(old_kernel,
                                                    new_kernel,
                                                    kernel_ops,
                                                    &manifest));
  }

  // Reorder the data blobs with the newly ordered manifest
  string ordered_blobs_path;
  TEST_AND_RETURN_FALSE(utils::MakeTempFile(
      "/tmp/CrAU_temp_data.ordered.XXXXXX",
      &ordered_blobs_path,
      NULL));
  ScopedPathUnlinker ordered_blobs_unlinker(ordered_blobs_path);
  TEST_AND_RETURN_FALSE(ReorderDataBlobs(&manifest,
                                         temp_file_path,
                                         ordered_blobs_path));
  temp_file_unlinker.reset();

  // Fill in the legacy noop_operations list.
  ProceduresToNoops(&manifest);

  // Check that install op blobs are in order.
  uint64_t next_blob_offset = 0;
  {
    for (int i = 0; i < (manifest.partition_operations_size() +
                         manifest.noop_operations_size()); i++) {
      InstallOperation* op =
          i < manifest.partition_operations_size() ?
          manifest.mutable_partition_operations(i) :
          manifest.mutable_noop_operations(
              i - manifest.partition_operations_size());
      if (op->has_data_offset()) {
        if (op->data_offset() != next_blob_offset) {
          LOG(FATAL) << "bad blob offset! " << op->data_offset() << " != "
                     << next_blob_offset;
        }
        next_blob_offset += op->data_length();
      }
    }
  }

  // Signatures appear at the end of the blobs. Note the offset in the
  // manifest
  if (!private_key_path.empty()) {
    uint64_t signature_blob_length = 0;
    TEST_AND_RETURN_FALSE(
        PayloadSigner::SignatureBlobLength(vector<string>(1, private_key_path),
                                           &signature_blob_length));
    AddSignatureOp(next_blob_offset, signature_blob_length, manifest);
  }

  TEST_AND_RETURN_FALSE(InitializePartitionInfos(old_image,
                                                 new_image,
                                                 manifest));

  // Serialize protobuf
  string serialized_manifest;

  CheckGraph(graph);
  TEST_AND_RETURN_FALSE(manifest.AppendToString(&serialized_manifest));
  CheckGraph(graph);

  LOG(INFO) << "Writing final delta file header...";
  DirectFileWriter writer(output_path.c_str());
  TEST_AND_RETURN_FALSE_ERRNO(writer.Open() == 0);
  ScopedFileWriterCloser writer_closer(&writer);

  // Write header
  TEST_AND_RETURN_FALSE(writer.Write(kDeltaMagic, strlen(kDeltaMagic)));

  // Write version number
  TEST_AND_RETURN_FALSE(WriteUint64AsBigEndian(&writer, kDeltaVersion));

  // Write protobuf length
  TEST_AND_RETURN_FALSE(WriteUint64AsBigEndian(&writer,
                                               serialized_manifest.size()));

  // Write protobuf
  LOG(INFO) << "Writing final delta file protobuf... "
            << serialized_manifest.size();
  TEST_AND_RETURN_FALSE(writer.Write(serialized_manifest.data(),
                                     serialized_manifest.size()));

  // Append the data blobs
  LOG(INFO) << "Writing final delta file data blobs...";
  int blobs_fd = open(ordered_blobs_path.c_str(), O_RDONLY, 0);
  files::ScopedFD blobs_fd_closer(blobs_fd);
  TEST_AND_RETURN_FALSE(blobs_fd >= 0);
  for (;;) {
    char buf[kBlockSize];
    ssize_t rc = read(blobs_fd, buf, sizeof(buf));
    if (0 == rc) {
      // EOF
      break;
    }
    TEST_AND_RETURN_FALSE_ERRNO(rc > 0);
    TEST_AND_RETURN_FALSE(writer.Write(buf, rc));
  }

  // Write signature blob.
  if (!private_key_path.empty()) {
    LOG(INFO) << "Signing the update...";
    vector<char> signature_blob;
    TEST_AND_RETURN_FALSE(PayloadSigner::SignPayload(
        output_path,
        vector<string>(1, private_key_path),
        &signature_blob));
    TEST_AND_RETURN_FALSE(writer.Write(&signature_blob[0],
                                       signature_blob.size()));
  }

  *metadata_size =
      strlen(kDeltaMagic) + 2 * sizeof(uint64_t) + serialized_manifest.size();
  ReportPayloadUsage(manifest, *metadata_size, op_name_map);

  LOG(INFO) << "All done. Successfully created delta file with "
            << "metadata size = " << *metadata_size;
  return true;
}

// Runs the bsdiff tool on two files and returns the resulting delta in
// 'out'. Returns true on success.
bool DeltaDiffGenerator::BsdiffFiles(const string& old_file,
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
  TEST_AND_RETURN_FALSE(Subprocess::SynchronousExec(cmd, &rc, NULL));
  TEST_AND_RETURN_FALSE(rc == 0);
  TEST_AND_RETURN_FALSE(utils::ReadFile(patch_file_path, out));
  unlink(patch_file_path.c_str());
  return true;
}

// The |blocks| vector contains a reader and writer for each block on the
// filesystem that's being in-place updated. We populate the reader/writer
// fields of |blocks| by calling this function.
// For each block in |operation| that is read or written, find that block
// in |blocks| and set the reader/writer field to the vertex passed.
// |graph| is not strictly necessary, but useful for printing out
// error messages.
bool DeltaDiffGenerator::AddInstallOpToBlocksVector(
    const InstallOperation& operation,
    const Graph& graph,
    Vertex::Index vertex,
    vector<Block>* blocks) {
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

void DeltaDiffGenerator::AddSignatureOp(uint64_t signature_blob_offset,
                                        uint64_t signature_blob_length,
                                        DeltaArchiveManifest& manifest) {
  LOG(INFO) << "Making room for signature in file";
  manifest.set_signatures_offset(signature_blob_offset);
  LOG(INFO) << "set? " << manifest.has_signatures_offset();
  // Add a dummy op at the end to appease older clients
  InstallOperation* dummy_op =
      manifest.add_noop_operations();
  dummy_op->set_type(InstallOperation_Type_REPLACE);
  dummy_op->set_data_offset(signature_blob_offset);
  manifest.set_signatures_offset(signature_blob_offset);
  dummy_op->set_data_length(signature_blob_length);
  manifest.set_signatures_size(signature_blob_length);
  Extent* dummy_extent = dummy_op->add_dst_extents();
  // Tell the dummy op to write this data to a big sparse hole
  dummy_extent->set_start_block(kSparseHole);
  dummy_extent->set_num_blocks((signature_blob_length + kBlockSize - 1) /
                               kBlockSize);
}

const char* const kBsdiffPath = "bsdiff";
const char* const kBspatchPath = "bspatch";

};  // namespace chromeos_update_engine
