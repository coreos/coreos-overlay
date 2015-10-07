// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>

#include <string>

#include <google/protobuf/repeated_field.h>
#include <gtest/gtest.h>

#include "update_engine/delta_performer.h"
#include "update_engine/extent_ranges.h"
#include "update_engine/graph_types.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

using std::string;

TEST(DeltaPerformerTest, ExtentsToByteStringTest) {
  uint64_t test[] = {1, 1, 4, 2, kSparseHole, 1, 0, 1};
  static_assert(arraysize(test) % 2 == 0, "array size uneven");
  const uint64_t block_size = 4096;
  const uint64_t file_length = 5 * block_size - 13;

  google::protobuf::RepeatedPtrField<Extent> extents;
  for (size_t i = 0; i < arraysize(test); i += 2) {
    Extent* extent = extents.Add();
    extent->set_start_block(test[i]);
    extent->set_num_blocks(test[i + 1]);
  }

  string expected_output = "4096:4096,16384:8192,-1:4096,0:4083";
  string actual_output;
  EXPECT_TRUE(DeltaPerformer::ExtentsToBsdiffPositionsString(extents,
                                                             block_size,
                                                             file_length,
                                                             &actual_output));
  EXPECT_EQ(expected_output, actual_output);
}

TEST(DeltaPerformerTest, IsIdempotentOperationTest) {
  InstallOperation op;
  EXPECT_TRUE(DeltaPerformer::IsIdempotentOperation(op));
  *(op.add_dst_extents()) = ExtentForRange(0, 5);
  EXPECT_TRUE(DeltaPerformer::IsIdempotentOperation(op));
  *(op.add_src_extents()) = ExtentForRange(4, 1);
  EXPECT_FALSE(DeltaPerformer::IsIdempotentOperation(op));
  op.clear_src_extents();
  *(op.add_src_extents()) = ExtentForRange(5, 3);
  EXPECT_TRUE(DeltaPerformer::IsIdempotentOperation(op));
  *(op.add_dst_extents()) = ExtentForRange(20, 6);
  EXPECT_TRUE(DeltaPerformer::IsIdempotentOperation(op));
  *(op.add_src_extents()) = ExtentForRange(19, 2);
  EXPECT_FALSE(DeltaPerformer::IsIdempotentOperation(op));
}

}  // namespace chromeos_update_engine
