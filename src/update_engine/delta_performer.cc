// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/delta_performer.h"

#include <errno.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/repeated_field.h>

#include "files/scoped_file.h"
#include "strings/string_printf.h"
#include "update_engine/bzip_extent_writer.h"
#include "update_engine/delta_metadata.h"
#include "update_engine/extent_ranges.h"
#include "update_engine/extent_writer.h"
#include "update_engine/file_writer.h"
#include "update_engine/graph_types.h"
#include "update_engine/payload_processor.h"
#include "update_engine/prefs_interface.h"
#include "update_engine/subprocess.h"
#include "update_engine/terminator.h"

using std::min;
using std::string;
using std::vector;
using google::protobuf::RepeatedPtrField;
using strings::StringPrintf;

namespace chromeos_update_engine {

namespace {

// Opens path for read/write, put the fd into *fd. On success returns true
// and sets *err to 0. On failure, returns false and sets *err to errno.
bool OpenFile(const char* path, int* fd, int* err) {
  if (*fd != -1) {
    LOG(ERROR) << "Can't open(" << path << "), *fd != -1 (it's " << *fd << ")";
    *err = EINVAL;
    return false;
  }
  *fd = open(path, O_RDWR, 000);
  if (*fd < 0) {
    *err = errno;
    PLOG(ERROR) << "Unable to open file " << path;
    return false;
  }
  *err = 0;
  return true;
}

}  // namespace {}

// Returns true if |op| is idempotent -- i.e., if we can interrupt it and repeat
// it safely. Returns false otherwise.
bool DeltaPerformer::IsIdempotentOperation(
    const InstallOperation& op) {
  if (op.src_extents_size() == 0) {
    return true;
  }
  // When in doubt, it's safe to declare an op non-idempotent. Note that we
  // could detect other types of idempotent operations here such as a MOVE that
  // moves blocks onto themselves. However, we rely on the server to not send
  // such operations at all.
  ExtentRanges src_ranges;
  src_ranges.AddRepeatedExtents(op.src_extents());
  const uint64_t block_count = src_ranges.blocks();
  src_ranges.SubtractRepeatedExtents(op.dst_extents());
  return block_count == src_ranges.blocks();
}

int DeltaPerformer::Open() {
  int err;
  OpenFile(install_plan_->install_path.c_str(), &fd_, &err);
  return -err;
}

int DeltaPerformer::Close() {
  int err = 0;
  if (close(fd_) == -1) {
    err = errno;
    PLOG(ERROR) << "Unable to close partition fd: " << fd_;
  }
  fd_ = -2;  // Set to invalid so that calls to Open() will fail.
  return -err;
}

ActionExitCode DeltaPerformer::PerformOperation(
    const InstallOperation& operation,
    const vector<char>& data) {

  ActionExitCode error = ValidateOperationHash(operation, data);
  if (error != kActionCodeSuccess) {
    if (install_plan_->hash_checks_mandatory) {
      LOG(ERROR) << "Mandatory operation hash check failed";
      return error;
    }

    // For non-mandatory cases, just log a warning.
    LOG(WARNING) << "Ignoring operation validation errors";
  }

  // Log every thousandth operation, and also the first and last ones
  if (operation.type() == InstallOperation_Type_REPLACE ||
      operation.type() == InstallOperation_Type_REPLACE_BZ) {
    if (!PerformReplaceOperation(operation, data)) {
      LOG(ERROR) << "Failed to perform replace operation";
      return kActionCodeDownloadOperationExecutionError;
    }
  } else if (operation.type() == InstallOperation_Type_MOVE) {
    if (!PerformMoveOperation(operation)) {
      LOG(ERROR) << "Failed to perform move operation";
      return kActionCodeDownloadOperationExecutionError;
    }
  } else if (operation.type() == InstallOperation_Type_BSDIFF) {
    if (!PerformBsdiffOperation(operation, data)) {
      LOG(ERROR) << "Failed to perform bsdiff operation";
      return kActionCodeDownloadOperationExecutionError;
    }
  } else {
    NOTREACHED();
  }

  return kActionCodeSuccess;
}

bool DeltaPerformer::PerformReplaceOperation(
    const InstallOperation& operation,
    const vector<char>& data) {
  CHECK(operation.type() == \
        InstallOperation_Type_REPLACE || \
        operation.type() == \
        InstallOperation_Type_REPLACE_BZ);

  TEST_AND_RETURN_FALSE(data.size() >= operation.data_length());

  DirectExtentWriter direct_writer;
  ZeroPadExtentWriter zero_pad_writer(&direct_writer);
  std::unique_ptr<BzipExtentWriter> bzip_writer;

  // Since bzip decompression is optional, we have a variable writer that will
  // point to one of the ExtentWriter objects above.
  ExtentWriter* writer = NULL;
  if (operation.type() == InstallOperation_Type_REPLACE) {
    writer = &zero_pad_writer;
  } else if (operation.type() ==
             InstallOperation_Type_REPLACE_BZ) {
    bzip_writer.reset(new BzipExtentWriter(&zero_pad_writer));
    writer = bzip_writer.get();
  } else {
    NOTREACHED();
  }

  // Create a vector of extents to pass to the ExtentWriter.
  vector<Extent> extents;
  for (int i = 0; i < operation.dst_extents_size(); i++) {
    extents.push_back(operation.dst_extents(i));
  }

  DCHECK(block_size_);
  TEST_AND_RETURN_FALSE(writer->Init(fd_, extents, block_size_));
  TEST_AND_RETURN_FALSE(writer->Write(&data[0], operation.data_length()));
  TEST_AND_RETURN_FALSE(writer->End());
  return true;
}

bool DeltaPerformer::PerformMoveOperation(const InstallOperation& operation) {
  // Sanity check the operation definition.
  TEST_AND_RETURN_FALSE(operation.data_length() == 0);

  // Calculate buffer size. Note, this function doesn't do a sliding
  // window to copy in case the source and destination blocks overlap.
  // If we wanted to do a sliding window, we could program the server
  // to generate deltas that effectively did a sliding window.

  uint64_t blocks_to_read = 0;
  for (int i = 0; i < operation.src_extents_size(); i++)
    blocks_to_read += operation.src_extents(i).num_blocks();

  uint64_t blocks_to_write = 0;
  for (int i = 0; i < operation.dst_extents_size(); i++)
    blocks_to_write += operation.dst_extents(i).num_blocks();

  DCHECK(block_size_);
  DCHECK_EQ(blocks_to_write, blocks_to_read);
  vector<char> buf(blocks_to_write * block_size_);

  // Read in bytes.
  ssize_t bytes_read = 0;
  for (int i = 0; i < operation.src_extents_size(); i++) {
    ssize_t bytes_read_this_iteration = 0;
    const Extent& extent = operation.src_extents(i);
    TEST_AND_RETURN_FALSE(utils::PReadAll(fd_,
                                          &buf[bytes_read],
                                          extent.num_blocks() * block_size_,
                                          extent.start_block() * block_size_,
                                          &bytes_read_this_iteration));
    TEST_AND_RETURN_FALSE(
        bytes_read_this_iteration ==
        static_cast<ssize_t>(extent.num_blocks() * block_size_));
    bytes_read += bytes_read_this_iteration;
  }

  // If this is a non-idempotent operation, request a delayed exit and clear the
  // update state in case the operation gets interrupted. Do this as late as
  // possible.
  if (!IsIdempotentOperation(operation)) {
    Terminator::set_exit_blocked(true);
    PayloadProcessor::ResetUpdateProgress(prefs_, true);
  }

  // Write bytes out.
  ssize_t bytes_written = 0;
  for (int i = 0; i < operation.dst_extents_size(); i++) {
    const Extent& extent = operation.dst_extents(i);
    TEST_AND_RETURN_FALSE(utils::PWriteAll(fd_,
                                           &buf[bytes_written],
                                           extent.num_blocks() * block_size_,
                                           extent.start_block() * block_size_));
    bytes_written += extent.num_blocks() * block_size_;
  }
  DCHECK_EQ(bytes_written, bytes_read);
  DCHECK_EQ(bytes_written, static_cast<ssize_t>(buf.size()));
  return true;
}

bool DeltaPerformer::ExtentsToBsdiffPositionsString(
    const RepeatedPtrField<Extent>& extents,
    uint64_t block_size,
    uint64_t full_length,
    string* positions_string) {
  string ret;
  uint64_t length = 0;
  for (int i = 0; i < extents.size(); i++) {
    Extent extent = extents.Get(i);
    int64_t start = extent.start_block();
    uint64_t this_length = min(full_length - length,
                               extent.num_blocks() * block_size);
    if (start == static_cast<int64_t>(kSparseHole))
      start = -1;
    else
      start *= block_size;
    ret += StringPrintf("%" PRIi64 ":%" PRIu64 ",", start, this_length);
    length += this_length;
  }
  TEST_AND_RETURN_FALSE(length == full_length);
  if (!ret.empty())
    ret.resize(ret.size() - 1);  // Strip trailing comma off
  *positions_string = ret;
  return true;
}

bool DeltaPerformer::PerformBsdiffOperation(
    const InstallOperation& operation,
    const vector<char>& data) {
  TEST_AND_RETURN_FALSE(data.size() >= operation.data_length());

  DCHECK(block_size_);
  string input_positions;
  TEST_AND_RETURN_FALSE(ExtentsToBsdiffPositionsString(operation.src_extents(),
                                                       block_size_,
                                                       operation.src_length(),
                                                       &input_positions));
  string output_positions;
  TEST_AND_RETURN_FALSE(ExtentsToBsdiffPositionsString(operation.dst_extents(),
                                                       block_size_,
                                                       operation.dst_length(),
                                                       &output_positions));

  string temp_filename;
  TEST_AND_RETURN_FALSE(utils::MakeTempFile("/tmp/au_patch.XXXXXX",
                                            &temp_filename,
                                            NULL));
  ScopedPathUnlinker path_unlinker(temp_filename);
  {
    int fd = open(temp_filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    files::ScopedFD fd_closer(fd);
    TEST_AND_RETURN_FALSE(
        utils::WriteAll(fd, &data[0], operation.data_length()));
  }

  const string& path = StringPrintf("/dev/fd/%d", fd_);

  // If this is a non-idempotent operation, request a delayed exit and clear the
  // update state in case the operation gets interrupted. Do this as late as
  // possible.
  if (!IsIdempotentOperation(operation)) {
    Terminator::set_exit_blocked(true);
    PayloadProcessor::ResetUpdateProgress(prefs_, true);
  }

  vector<string> cmd;
  cmd.push_back(kBspatchPath);
  cmd.push_back(path);
  cmd.push_back(path);
  cmd.push_back(temp_filename);
  cmd.push_back(input_positions);
  cmd.push_back(output_positions);
  int return_code = 0;
  TEST_AND_RETURN_FALSE(
      Subprocess::SynchronousExecFlags(cmd,
                                       G_SPAWN_LEAVE_DESCRIPTORS_OPEN,
                                       &return_code,
                                       NULL));
  TEST_AND_RETURN_FALSE(return_code == 0);

  if (operation.dst_length() % block_size_) {
    // Zero out rest of final block.
    // TODO(adlr): build this into bspatch; it's more efficient that way.
    const Extent& last_extent =
        operation.dst_extents(operation.dst_extents_size() - 1);
    const uint64_t end_byte =
        (last_extent.start_block() + last_extent.num_blocks()) * block_size_;
    const uint64_t begin_byte =
        end_byte - (block_size_ - operation.dst_length() % block_size_);
    vector<char> zeros(end_byte - begin_byte);
    TEST_AND_RETURN_FALSE(
        utils::PWriteAll(fd_, &zeros[0], end_byte - begin_byte, begin_byte));
  }

  return true;
}

ActionExitCode DeltaPerformer::ValidateOperationHash(
    const InstallOperation& operation,
    const vector<char>& data) {

  if (!operation.data_sha256_hash().size()) {
    if (!operation.data_length()) {
      // Operations that do not have any data blob won't have any operation hash
      // either. So, these operations are always considered validated since the
      // metadata that contains all the non-data-blob portions of the operation
      // has already been validated. This is true for both HTTP and HTTPS cases.
      return kActionCodeSuccess;
    }

    // No hash is present for an operation that has data blobs. This shouldn't
    // happen normally for any client that has this code, because the
    // corresponding update should have been produced with the operation
    // hashes. So if it happens it means either we've turned operation hash
    // generation off in DeltaDiffGenerator or it's a regression of some sort.
    if (install_plan_->hash_checks_mandatory) {
      LOG(ERROR) << "Missing mandatory operation hash for operation";
      return kActionCodeDownloadOperationHashMissingError;
    }

    // For non-mandatory cases, just log a warning.
    LOG(WARNING) << "Cannot validate operation as there's no operation hash";

    return kActionCodeSuccess;
  }

  vector<char> expected_op_hash;
  expected_op_hash.assign(operation.data_sha256_hash().data(),
                          (operation.data_sha256_hash().data() +
                           operation.data_sha256_hash().size()));

  OmahaHashCalculator operation_hasher;
  operation_hasher.Update(&data[0], operation.data_length());
  if (!operation_hasher.Finalize()) {
    LOG(ERROR) << "Unable to compute actual hash of operation";
    return kActionCodeDownloadOperationHashVerificationError;
  }

  vector<char> calculated_op_hash = operation_hasher.raw_hash();
  if (calculated_op_hash != expected_op_hash) {
    LOG(ERROR) << "Hash verification failed for operation. Expected hash = ";
    utils::HexDumpVector(expected_op_hash);
    LOG(ERROR) << "Calculated hash over " << operation.data_length()
               << " bytes at offset: " << operation.data_offset() << " = ";
    utils::HexDumpVector(calculated_op_hash);
    return kActionCodeDownloadOperationHashMismatch;
  }

  return kActionCodeSuccess;
}

}  // namespace chromeos_update_engine
