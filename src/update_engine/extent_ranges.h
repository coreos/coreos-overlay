// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_EXTENT_RANGES_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_EXTENT_RANGES_H__

#include <map>
#include <set>
#include <vector>

#include <base/basictypes.h>

#include "update_engine/delta_diff_generator.h"
#include "update_engine/graph_types.h"
#include "update_engine/update_metadata.pb.h"

// An ExtentRanges object represents an unordered collection of extents (and
// therefore blocks). Such an object may be modified by adding or subtracting
// blocks (think: set addition or set subtraction). Note that ExtentRanges
// ignores sparse hole extents mostly to avoid confusion between extending a
// sparse hole range vs. set addition but also to ensure that the delta
// generator doesn't use sparse holes as scratch space.

namespace chromeos_update_engine {

struct ExtentLess {
  bool operator()(const Extent& x, const Extent& y) const {
    return x.start_block() < y.start_block();
  }
};

Extent ExtentForRange(uint64_t start_block, uint64_t num_blocks);

class ExtentRanges {
 public:
  typedef std::set<Extent, ExtentLess> ExtentSet;

  ExtentRanges() : blocks_(0) {}
  void AddBlock(uint64_t block);
  void SubtractBlock(uint64_t block);
  void AddExtent(Extent extent);
  void SubtractExtent(const Extent& extent);
  void AddExtents(const std::vector<Extent>& extents);
  void SubtractExtents(const std::vector<Extent>& extents);
  void AddRepeatedExtents(
      const ::google::protobuf::RepeatedPtrField<Extent> &exts);
  void SubtractRepeatedExtents(
      const ::google::protobuf::RepeatedPtrField<Extent> &exts);
  void AddRanges(const ExtentRanges& ranges);
  void SubtractRanges(const ExtentRanges& ranges);

  static bool ExtentsOverlapOrTouch(const Extent& a, const Extent& b);
  static bool ExtentsOverlap(const Extent& a, const Extent& b);

  // Dumps contents to the log file. Useful for debugging.
  void Dump() const;

  uint64_t blocks() const { return blocks_; }
  const ExtentSet& extent_set() const { return extent_set_; }

  // Returns an ordered vector of extents for |count| blocks,
  // using extents in extent_set_. The returned extents are not
  // removed from extent_set_. |count| must be less than or equal to
  // the number of blocks in this extent set.
  std::vector<Extent> GetExtentsForBlockCount(uint64_t count) const;

 private:
  ExtentSet extent_set_;
  uint64_t blocks_;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_EXTENT_RANGES_H__
