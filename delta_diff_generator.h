// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_DIFF_GENERATOR_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_DIFF_GENERATOR_H__

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "update_engine/graph_types.h"
#include "update_engine/update_metadata.pb.h"

// There is one function in DeltaDiffGenerator of importance to users
// of the class: GenerateDeltaUpdateFile(). Before calling it,
// the old and new images must be mounted. Call GenerateDeltaUpdateFile()
// with both the mount-points of the images in addition to the paths of
// the images (both old and new). A delta from old to new will be
// generated and stored in output_path.

namespace chromeos_update_engine {

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
  // output_path is the filename where the delta update should be written.
  // Returns true on success.
  static bool GenerateDeltaUpdateFile(const std::string& old_root,
                                      const std::string& old_image,
                                      const std::string& new_root,
                                      const std::string& new_image,
                                      const std::string& output_path);

  // These functions are public so that the unit tests can access them:

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
                             std::vector<char>* out_data,
                             DeltaArchiveManifest_InstallOperation* out_op);

  // Modifies blocks read by 'op' so that any blocks referred to by
  // 'remove_extents' are replaced with blocks from 'replace_extents'.
  // 'remove_extents' and 'replace_extents' must be the same number of blocks.
  // Blocks will be substituted in the order listed in the vectors.
  // E.g. if 'op' reads blocks 1, 2, 3, 4, 5, 6, 7, 8, remove_extents
  // contains blocks 6, 2, 3, 5, and replace blocks contains
  // 12, 13, 14, 15, then op will be changed to read from:
  // 1, 13, 14, 4, 15, 12, 7, 8
  static void SubstituteBlocks(DeltaArchiveManifest_InstallOperation* op,
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
                       const std::vector<Block>& blocks,
                       const std::set<Edge>& edges);

  // Stores all Extents in 'extents' into 'out'.
  static void StoreExtents(std::vector<Extent>& extents,
                           google::protobuf::RepeatedPtrField<Extent>* out);
                           
  // Creates all the edges for the graph. Writers of a block point to
  // readers of the same block. This is because for an edge A->B, B
  // must complete before A executes.
  static void CreateEdges(Graph* graph, const std::vector<Block>& blocks);
  
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

 private:
  // This should never be constructed
  DISALLOW_IMPLICIT_CONSTRUCTORS(DeltaDiffGenerator);
};

};  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DELTA_DIFF_GENERATOR_H__
