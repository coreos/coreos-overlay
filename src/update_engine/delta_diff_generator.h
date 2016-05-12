// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_DIFF_GENERATOR_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_DIFF_GENERATOR_H__

#include <string>
#include <vector>

#include "macros.h"
#include "update_engine/graph_types.h"
#include "update_engine/update_metadata.pb.h"

// There is one function in DeltaDiffGenerator of importance to users
// of the class: GenerateDeltaUpdateFile(). Before calling it,
// the old and new images must be mounted. Call GenerateDeltaUpdateFile()
// with both the mount-points of the images in addition to the paths of
// the images (both old and new). A delta from old to new will be
// generated and stored in output_path.

namespace chromeos_update_engine {

// This struct stores all relevant info for an edge that is cut between
// nodes old_src -> old_dst by creating new vertex new_vertex. The new
// relationship is:
// old_src -(read before)-> new_vertex <-(write before)- old_dst
// new_vertex is a MOVE operation that moves some existing blocks into
// temp space. The temp extents are, by necessity, stored in new_vertex
// (as dst extents) and old_dst (as src extents), but they are also broken
// out into tmp_extents, as the nodes themselves may contain many more
// extents.
struct CutEdgeVertexes {
  Vertex::Index new_vertex;
  Vertex::Index old_src;
  Vertex::Index old_dst;
  std::vector<Extent> tmp_extents;
};

class DeltaDiffGenerator {
 public:
  // Represents a disk block on the install partition.
  struct Block {
    // During install, each block on the install partition will be written
    // and some may be read (in all likelihood, many will be read).
    // The reading and writing will be performed by InstallOperations,
    // each of which has a corresponding vertex in a graph.
    // A Block object tells which vertex will read or write this block
    // at install time.
    // Generally, there will be a vector of Block objects whose length
    // is the number of blocks on the install partition.
    Block() : reader(Vertex::kInvalidIndex), writer(Vertex::kInvalidIndex) {}
    Vertex::Index reader;
    Vertex::Index writer;
  };

  // This is the only function that external users of the class should call.
  // old_image and new_image are paths to two image files. They should be
  // mounted read-only at paths old_root and new_root respectively.
  // private_key_path points to a private key used to sign the update.
  // Pass empty string to not sign the update.
  // output_path is the filename where the delta update should be written.
  // Returns true on success. Also writes the size of the metadata into
  // |metadata_size|.
  static bool GenerateDeltaUpdateFile(const std::string& old_root,
                                      const std::string& old_image,
                                      const std::string& new_root,
                                      const std::string& new_image,
                                      const std::string& output_path,
                                      const std::string& private_key_path,
                                      uint64_t* metadata_size);

  // These functions are public so that the unit tests can access them:

  // Takes a graph, which is not a DAG, which represents the files just
  // read from disk, and converts it into a DAG by breaking all cycles
  // and finding temp space to resolve broken edges.
  // The final order of the nodes is given in |final_order|
  // Some files may need to be reread from disk, thus |fd| and
  // |data_file_size| are be passed.
  // If |scratch_vertex| is not kInvalidIndex, removes it from
  // |final_order| before returning.
  // Returns true on success.
  static bool ConvertGraphToDag(Graph* graph,
                                const std::string& new_root,
                                int fd,
                                off_t* data_file_size,
                                std::vector<Vertex::Index>* final_order,
                                Vertex::Index scratch_vertex);

  // Reads old_filename (if it exists) and a new_filename and determines
  // the smallest way to encode this file for the diff. It stores
  // necessary data in out_data and fills in out_op.
  // If there's no change in old and new files, it creates a MOVE
  // operation. If there is a change, or the old file doesn't exist,
  // the smallest of REPLACE, REPLACE_BZ, or BSDIFF wins.
  // new_filename must contain at least one byte.
  // Returns true on success.
  static bool ReadFileToDiff(const std::string& old_filename,
                             const std::string& new_filename,
                             bool bsdiff_allowed,
                             std::vector<char>* out_data,
                             InstallOperation* out_op,
                             bool gather_extents);

  // Creates a dummy REPLACE_BZ node in the given |vertex|. This can be used
  // to provide scratch space. The node writes |num_blocks| blocks starting at
  // |start_block|The node should be marked invalid before writing all nodes to
  // the output file.
  static void CreateScratchNode(uint64_t start_block,
                                uint64_t num_blocks,
                                Vertex* vertex);

  // Modifies blocks read by 'op' so that any blocks referred to by
  // 'remove_extents' are replaced with blocks from 'replace_extents'.
  // 'remove_extents' and 'replace_extents' must be the same number of blocks.
  // Blocks will be substituted in the order listed in the vectors.
  // E.g. if 'op' reads blocks 1, 2, 3, 4, 5, 6, 7, 8, remove_extents
  // contains blocks 6, 2, 3, 5, and replace blocks contains
  // 12, 13, 14, 15, then op will be changed to read from:
  // 1, 13, 14, 4, 15, 12, 7, 8
  static void SubstituteBlocks(Vertex* vertex,
                               const std::vector<Extent>& remove_extents,
                               const std::vector<Extent>& replace_extents);

  // Cuts 'edges' from 'graph' according to the AU algorithm. This means
  // for each edge A->B, remove the dependency that B occur before A.
  // Do this by creating a new operation X that copies from the blocks
  // specified by the edge's properties to temp space T. Modify B to read
  // from T rather than the blocks in the edge. Modify A to depend on X,
  // but not on B. Free space is found by looking in 'blocks'.
  // Returns true on success.
  static bool CutEdges(Graph* graph,
                       const std::set<Edge>& edges,
                       std::vector<CutEdgeVertexes>* out_cuts);

  // Stores all Extents in 'extents' into 'out'.
  static void StoreExtents(const std::vector<Extent>& extents,
                           google::protobuf::RepeatedPtrField<Extent>* out);

  // Creates all the edges for the graph. Writers of a block point to
  // readers of the same block. This is because for an edge A->B, B
  // must complete before A executes.
  static void CreateEdges(Graph* graph, const std::vector<Block>& blocks);

  // Given a topologically sorted graph |op_indexes| and |graph|, alters
  // |op_indexes| to move all the full operations to the end of the vector.
  // Full operations should not be depended on, so this is safe.
  static void MoveFullOpsToBack(Graph* graph,
                                std::vector<Vertex::Index>* op_indexes);

  // Sorts the vector |cuts| by its |cuts[].old_dest| member. Order is
  // determined by the order of elements in op_indexes.
  static void SortCutsByTopoOrder(std::vector<Vertex::Index>& op_indexes,
                                  std::vector<CutEdgeVertexes>* cuts);

  // Returns true iff there are no extents in the graph that refer to temp
  // blocks. Temp blocks are in the range [kTempBlockStart, kSparseHole).
  static bool NoTempBlocksRemain(const Graph& graph);

  // Install operations in the manifest may reference data blobs, which
  // are in data_blobs_path. This function creates a new data blobs file
  // with the data blobs in the same order as the referencing install
  // operations in the manifest. E.g. if manifest[0] has a data blob
  // "X" at offset 1, manifest[1] has a data blob "Y" at offset 0,
  // and data_blobs_path's file contains "YX", new_data_blobs_path
  // will set to be a file that contains "XY".
  static bool ReorderDataBlobs(DeltaArchiveManifest* manifest,
                               const std::string& data_blobs_path,
                               const std::string& new_data_blobs_path);

  // Computes a SHA256 hash of the given buf and sets the hash value in the
  // operation so that update_engine could verify. This hash should be set
  // for all operations that have a non-zero data blob. One exception is the
  // dummy operation for signature blob because the contents of the signature
  // blob will not be available at payload creation time. So, update_engine will
  // gracefully ignore the dummy signature operation.
  static bool AddOperationHash(InstallOperation* op,
                               const std::vector<char>& buf);

  // Handles allocation of temp blocks to a cut edge by converting the
  // dest node to a full op. This removes the need for temp blocks, but
  // comes at the cost of a worse compression ratio.
  // For example, say we have A->B->A. It would first be cut to form:
  // A->B->N<-A, where N copies blocks to temp space. If there are no
  // temp blocks, this function can be called to convert it to the form:
  // A->B. Now, A is a full operation.
  static bool ConvertCutToFullOp(Graph* graph,
                                 const CutEdgeVertexes& cut,
                                 const std::string& new_root,
                                 int data_fd,
                                 off_t* data_file_size);

  // Takes |op_indexes|, which is effectively a mapping from order in
  // which the op is performed -> graph vertex index, and produces the
  // reverse: a mapping from graph vertex index -> op_indexes index.
  static void GenerateReverseTopoOrderMap(
      std::vector<Vertex::Index>& op_indexes,
      std::vector<std::vector<Vertex::Index>::size_type>* reverse_op_indexes);

  // Takes a |graph|, which has edges that must be cut, as listed in
  // |cuts|.  Cuts the edges. Maintains a list in which the operations
  // will be performed (in |op_indexes|) and the reverse (in
  // |reverse_op_indexes|).  Cutting edges requires scratch space, and
  // if insufficient scratch is found, the file is reread and will be
  // send down (either as REPLACE or REPLACE_BZ).  Returns true on
  // success.
  static bool AssignTempBlocks(
      Graph* graph,
      const std::string& new_root,
      int data_fd,
      off_t* data_file_size,
      std::vector<Vertex::Index>* op_indexes,
      std::vector<std::vector<Vertex::Index>::size_type>* reverse_op_indexes,
      const std::vector<CutEdgeVertexes>& cuts);

  // Returns true if |op| is a no-op operation that doesn't do any useful work
  // (e.g., a move operation that copies blocks onto themselves).
  static bool IsNoopOperation(const InstallOperation& op);

  static bool InitializePartitionInfo(const std::string& partition,
                                      InstallInfo* info);

  // Runs the bsdiff tool on two files and returns the resulting delta in
  // |out|. Returns true on success.
  static bool BsdiffFiles(const std::string& old_file,
                          const std::string& new_file,
                          std::vector<char>* out);

  // The |blocks| vector contains a reader and writer for each block on the
  // filesystem that's being in-place updated. We populate the reader/writer
  // fields of |blocks| by calling this function.
  // For each block in |operation| that is read or written, find that block
  // in |blocks| and set the reader/writer field to the vertex passed.
  // |graph| is not strictly necessary, but useful for printing out
  // error messages.
  static bool AddInstallOpToBlocksVector(
      const InstallOperation& operation,
      const Graph& graph,
      Vertex::Index vertex,
      std::vector<DeltaDiffGenerator::Block>* blocks);

  // Adds to |manifest| a dummy operation that points to a signature blob
  // located at the specified offset/length.
  static void AddSignatureOp(uint64_t signature_blob_offset,
                             uint64_t signature_blob_length,
                             DeltaArchiveManifest& manifest);

 private:
 // This should never be constructed
  DISALLOW_IMPLICIT_CONSTRUCTORS(DeltaDiffGenerator);
};

extern const char* const kBsdiffPath;
extern const char* const kBspatchPath;

};  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_DIFF_GENERATOR_H__
