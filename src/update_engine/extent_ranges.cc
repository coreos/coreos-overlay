// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/extent_ranges.h"

#include <set>
#include <utility>
#include <vector>

#include <glog/logging.h>

using std::max;
using std::min;
using std::set;
using std::vector;

namespace chromeos_update_engine {

bool ExtentRanges::ExtentsOverlapOrTouch(const Extent& a, const Extent& b) {
  if (a.start_block() == b.start_block())
    return true;
  if (a.start_block() == kSparseHole || b.start_block() == kSparseHole)
    return false;
  if (a.start_block() < b.start_block()) {
    return a.start_block() + a.num_blocks() >= b.start_block();
  } else {
    return b.start_block() + b.num_blocks() >= a.start_block();
  }
}

bool ExtentRanges::ExtentsOverlap(const Extent& a, const Extent& b) {
  if (a.start_block() == b.start_block())
    return true;
  if (a.start_block() == kSparseHole || b.start_block() == kSparseHole)
    return false;
  if (a.start_block() < b.start_block()) {
    return a.start_block() + a.num_blocks() > b.start_block();
  } else {
    return b.start_block() + b.num_blocks() > a.start_block();
  }
}

void ExtentRanges::AddBlock(uint64_t block) {
  AddExtent(ExtentForRange(block, 1));
}

void ExtentRanges::SubtractBlock(uint64_t block) {
  SubtractExtent(ExtentForRange(block, 1));
}

namespace {

Extent UnionOverlappingExtents(const Extent& first, const Extent& second) {
  CHECK_NE(kSparseHole, first.start_block());
  CHECK_NE(kSparseHole, second.start_block());
  uint64_t start = min(first.start_block(), second.start_block());
  uint64_t end = max(first.start_block() + first.num_blocks(),
                     second.start_block() + second.num_blocks());
  return ExtentForRange(start, end - start);
}

}  // namespace {}

void ExtentRanges::AddExtent(Extent extent) {
  if (extent.start_block() == kSparseHole || extent.num_blocks() == 0)
    return;

  ExtentSet::iterator begin_del = extent_set_.end();
  ExtentSet::iterator end_del = extent_set_.end();
  uint64_t del_blocks = 0;
  for (ExtentSet::iterator it = extent_set_.begin(), e = extent_set_.end();
       it != e; ++it) {
    if (ExtentsOverlapOrTouch(*it, extent)) {
      end_del = it;
      ++end_del;
      del_blocks += it->num_blocks();
      if (begin_del == extent_set_.end())
        begin_del = it;

      extent = UnionOverlappingExtents(extent, *it);
    }
  }
  extent_set_.erase(begin_del, end_del);
  extent_set_.insert(extent);
  blocks_ -= del_blocks;
  blocks_ += extent.num_blocks();
}

namespace {
// Returns base - subtractee (set subtraction).
ExtentRanges::ExtentSet SubtractOverlappingExtents(const Extent& base,
                                                   const Extent& subtractee) {
  ExtentRanges::ExtentSet ret;
  if (subtractee.start_block() > base.start_block()) {
    ret.insert(ExtentForRange(base.start_block(),
                              subtractee.start_block() - base.start_block()));
  }
  uint64_t base_end = base.start_block() + base.num_blocks();
  uint64_t subtractee_end = subtractee.start_block() + subtractee.num_blocks();
  if (base_end > subtractee_end) {
    ret.insert(ExtentForRange(subtractee_end, base_end - subtractee_end));
  }
  return ret;
}
}  // namespace {}

void ExtentRanges::SubtractExtent(const Extent& extent) {
  if (extent.start_block() == kSparseHole || extent.num_blocks() == 0)
    return;

  ExtentSet::iterator begin_del = extent_set_.end();
  ExtentSet::iterator end_del = extent_set_.end();
  uint64_t del_blocks = 0;
  ExtentSet new_extents;
  for (ExtentSet::iterator it = extent_set_.begin(), e = extent_set_.end();
       it != e; ++it) {
    if (!ExtentsOverlap(*it, extent))
      continue;

    if (begin_del == extent_set_.end())
      begin_del = it;
    end_del = it;
    ++end_del;

    del_blocks += it->num_blocks();

    ExtentSet subtraction = SubtractOverlappingExtents(*it, extent);
    for (ExtentSet::iterator jt = subtraction.begin(), je = subtraction.end();
         jt != je; ++jt) {
      new_extents.insert(*jt);
      del_blocks -= jt->num_blocks();
    }
  }
  extent_set_.erase(begin_del, end_del);
  extent_set_.insert(new_extents.begin(), new_extents.end());
  blocks_ -= del_blocks;
}

void ExtentRanges::AddRanges(const ExtentRanges& ranges) {
  for (ExtentSet::const_iterator it = ranges.extent_set_.begin(),
           e = ranges.extent_set_.end(); it != e; ++it) {
    AddExtent(*it);
  }
}

void ExtentRanges::SubtractRanges(const ExtentRanges& ranges) {
  for (ExtentSet::const_iterator it = ranges.extent_set_.begin(),
           e = ranges.extent_set_.end(); it != e; ++it) {
    SubtractExtent(*it);
  }
}

void ExtentRanges::AddExtents(const vector<Extent>& extents) {
  for (vector<Extent>::const_iterator it = extents.begin(), e = extents.end();
       it != e; ++it) {
    AddExtent(*it);
  }
}

void ExtentRanges::SubtractExtents(const vector<Extent>& extents) {
  for (vector<Extent>::const_iterator it = extents.begin(), e = extents.end();
       it != e; ++it) {
    SubtractExtent(*it);
  }
}

void ExtentRanges::AddRepeatedExtents(
    const ::google::protobuf::RepeatedPtrField<Extent> &exts) {
  for (int i = 0, e = exts.size(); i != e; ++i) {
    AddExtent(exts.Get(i));
  }
}

void ExtentRanges::SubtractRepeatedExtents(
    const ::google::protobuf::RepeatedPtrField<Extent> &exts) {
  for (int i = 0, e = exts.size(); i != e; ++i) {
    SubtractExtent(exts.Get(i));
  }
}

void ExtentRanges::Dump() const {
  LOG(INFO) << "ExtentRanges Dump. blocks: " << blocks_;
  for (ExtentSet::const_iterator it = extent_set_.begin(),
           e = extent_set_.end();
       it != e; ++it) {
    LOG(INFO) << "{" << it->start_block() << ", " << it->num_blocks() << "}";
  }
}

Extent ExtentForRange(uint64_t start_block, uint64_t num_blocks) {
  Extent ret;
  ret.set_start_block(start_block);
  ret.set_num_blocks(num_blocks);
  return ret;
}

std::vector<Extent> ExtentRanges::GetExtentsForBlockCount(
    uint64_t count) const {
  vector<Extent> out;
  if (count == 0)
    return out;
  uint64_t out_blocks = 0;
  CHECK(count <= blocks_);
  for (ExtentSet::const_iterator it = extent_set_.begin(),
           e = extent_set_.end();
       it != e; ++it) {
    const uint64_t blocks_needed = count - out_blocks;
    const Extent& extent = *it;
    out.push_back(extent);
    out_blocks += extent.num_blocks();
    if (extent.num_blocks() < blocks_needed)
      continue;
    if (extent.num_blocks() == blocks_needed)
      break;
    // If we get here, we just added the last extent needed, but it's too big
    out_blocks -= extent.num_blocks();
    out_blocks += blocks_needed;
    out.back().set_num_blocks(blocks_needed);
    break;
  }
  return out;
}

}  // namespace chromeos_update_engine
