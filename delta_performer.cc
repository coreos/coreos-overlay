// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/delta_performer.h"
#include <endian.h>
#include <errno.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include <google/protobuf/repeated_field.h>

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "update_engine/bzip_extent_writer.h"
#include "update_engine/delta_diff_generator.h"
#include "update_engine/extent_writer.h"
#include "update_engine/graph_types.h"
#include "update_engine/subprocess.h"

using std::min;
using std::string;
using std::vector;
using google::protobuf::RepeatedPtrField;

namespace chromeos_update_engine {

namespace {

const int kDeltaVersionLength = 8;
const int kDeltaProtobufLengthLength = 8;

// Remove count bytes from the beginning of *buffer.
void RemoveBufferHeadBytes(vector<char>* buffer, size_t count) {
  buffer->erase(buffer->begin(), buffer->begin() + count);
}

// Converts extents to a human-readable string, for use by DumpUpdateProto().
string ExtentsToString(const RepeatedPtrField<Extent>& extents) {
  string ret;
  for (int i = 0; i < extents.size(); i++) {
    const Extent& extent = extents.Get(i);
    if (extent.start_block() == kSparseHole) {
      ret += StringPrintf("{kSparseHole, %" PRIu64 "}, ", extent.num_blocks());
    } else {
      ret += StringPrintf("{%" PRIu64 ", %" PRIu64 "}, ",
                          extent.start_block(), extent.num_blocks());
    }
  }
  if (!ret.empty()) {
    DCHECK_GT(ret.size(), static_cast<size_t>(1));
    ret.resize(ret.size() - 2);
  }
  return ret;
}

// LOGs a DeltaArchiveManifest object. Useful for debugging.
void DumpUpdateProto(const DeltaArchiveManifest& manifest) {
  LOG(INFO) << "Update Proto:";
  LOG(INFO) << "  src_checksum: " << manifest.src_checksum();
  LOG(INFO) << "  dst_checksum: " << manifest.dst_checksum();
  LOG(INFO) << "  block_size: " << manifest.block_size();
  for (int i = 0; i < manifest.install_operations_size(); i++) {
    const DeltaArchiveManifest_InstallOperation& op =
        manifest.install_operations(i);
    LOG(INFO) << "  operation(" << i << ")";
    LOG(INFO) << "    type: "
              << DeltaArchiveManifest_InstallOperation_Type_Name(op.type());
    if (op.has_data_offset())
      LOG(INFO) << "    data_offset: " << op.data_offset();
    if (op.has_data_length())
      LOG(INFO) << "    data_length: " << op.data_length();
    LOG(INFO) << "    src_extents: " << ExtentsToString(op.src_extents());
    if (op.has_src_length())
      LOG(INFO) << "    src_length: " << op.src_length();
    LOG(INFO) << "    dst_extents: " << ExtentsToString(op.dst_extents());
    if (op.has_dst_length())
      LOG(INFO) << "    dst_length: " << op.dst_length();
  }
}
}  // namespace {}

int DeltaPerformer::Open(const char* path, int flags, mode_t mode) {
  if (fd_ != -1) {
    LOG(ERROR) << "Can't Open(), fd_ != -1 (it's " << fd_ << ")";
    return -EINVAL;
  }
  path_ = path;
  fd_ = open(path, O_RDWR, 000);
  if (fd_ < 0)
    return -errno;
  return 0;
}

int DeltaPerformer::Close() {
  if (!buffer_.empty()) {
    LOG(ERROR) << "Called Close() while buffer not empty!";
    return -1;
  }
  if (close(fd_) == -1)
    return -errno;
  fd_ = -2;  // Set so that isn't not valid AND calls to Open() will fail.
  path_ = "";
  return 0;
}

// Wrapper around write. Returns bytes written on success or
// -errno on error.
// This function performs as many actions as it can, given the amount of
// data received thus far.
int DeltaPerformer::Write(const void* bytes, size_t count) {
  const char* c_bytes = reinterpret_cast<const char*>(bytes);
  buffer_.insert(buffer_.end(), c_bytes, c_bytes + count);

  if (!manifest_valid_) {
    // See if we have enough bytes for the manifest yet
    if (buffer_.size() < strlen(kDeltaMagic) +
        kDeltaVersionLength + kDeltaProtobufLengthLength) {
      // Don't have enough bytes to even know the protobuf length
      return count;
    }
    uint64_t protobuf_length;
    COMPILE_ASSERT(sizeof(protobuf_length) == kDeltaProtobufLengthLength,
                   protobuf_length_size_mismatch);
    memcpy(&protobuf_length,
           &buffer_[strlen(kDeltaMagic) + kDeltaVersionLength],
           kDeltaProtobufLengthLength);
    protobuf_length = be64toh(protobuf_length);  // switch big endian to host
    if (buffer_.size() < strlen(kDeltaMagic) + kDeltaVersionLength +
        kDeltaProtobufLengthLength + protobuf_length) {
      return count;
    }
    // We have the full proto buffer in buffer_. Parse it.
    const int offset = strlen(kDeltaMagic) + kDeltaVersionLength +
        kDeltaProtobufLengthLength;
    if (!manifest_.ParseFromArray(&buffer_[offset], protobuf_length)) {
      LOG(ERROR) << "Unable to parse manifest in update file.";
      return -EINVAL;
    }
    // Remove protobuf and header info from buffer_, so buffer_ contains
    // just data blobs
    RemoveBufferHeadBytes(&buffer_,
                          strlen(kDeltaMagic) +
                          kDeltaVersionLength +
                          kDeltaProtobufLengthLength + protobuf_length);
    manifest_valid_ = true;
    block_size_ = manifest_.block_size();
  }
  while (next_operation_ < manifest_.install_operations_size() &&
         CanPerformInstallOperation(
             manifest_.install_operations(next_operation_))) {
    const DeltaArchiveManifest_InstallOperation &op =
        manifest_.install_operations(next_operation_);
    if (op.type() == DeltaArchiveManifest_InstallOperation_Type_REPLACE ||
        op.type() == DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ) {
      if (!PerformReplaceOperation(op)) {
        LOG(ERROR) << "Failed to perform replace operation " << next_operation_;
        return -EINVAL;
      }
    } else if (op.type() == DeltaArchiveManifest_InstallOperation_Type_MOVE) {
      if (!PerformMoveOperation(op)) {
        LOG(ERROR) << "Failed to perform move operation " << next_operation_;
        return -EINVAL;
      }
    } else if (op.type() == DeltaArchiveManifest_InstallOperation_Type_BSDIFF) {
      if (!PerformBsdiffOperation(op)) {
        LOG(ERROR) << "Failed to perform bsdiff operation " << next_operation_;
        return -EINVAL;
      }
    }
    next_operation_++;
  }
  return count;
}

bool DeltaPerformer::CanPerformInstallOperation(
    const chromeos_update_engine::DeltaArchiveManifest_InstallOperation&
    operation) {
  // Move operations don't require any data blob, so they can always
  // be performed
  if (operation.type() == DeltaArchiveManifest_InstallOperation_Type_MOVE)
    return true;

  // See if we have the entire data blob in the buffer
  if (operation.data_offset() < buffer_offset_) {
    LOG(ERROR) << "we threw away data it seems?";
    return false;
  }
  
  return (operation.data_offset() + operation.data_length()) <=
      (buffer_offset_ + buffer_.size());
}

bool DeltaPerformer::PerformReplaceOperation(
    const DeltaArchiveManifest_InstallOperation& operation) {
  CHECK(operation.type() == \
        DeltaArchiveManifest_InstallOperation_Type_REPLACE || \
        operation.type() == \
        DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ);

  // Since we delete data off the beginning of the buffer as we use it,
  // the data we need should be exactly at the beginning of the buffer.
  CHECK_EQ(buffer_offset_, operation.data_offset());
  CHECK_GE(buffer_.size(), operation.data_length());
  
  DirectExtentWriter direct_writer;
  ZeroPadExtentWriter zero_pad_writer(&direct_writer);
  scoped_ptr<BzipExtentWriter> bzip_writer;
  
  // Since bzip decompression is optional, we have a variable writer that will
  // point to one of the ExtentWriter objects above.
  ExtentWriter* writer = NULL;
  if (operation.type() == DeltaArchiveManifest_InstallOperation_Type_REPLACE) {
    writer = &zero_pad_writer;
  } else if (operation.type() ==
             DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ) {
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

  TEST_AND_RETURN_FALSE(writer->Init(fd_, extents, block_size_));
  TEST_AND_RETURN_FALSE(writer->Write(&buffer_[0], operation.data_length()));
  TEST_AND_RETURN_FALSE(writer->End());
  
  // Update buffer
  buffer_offset_ += operation.data_length();
  RemoveBufferHeadBytes(&buffer_, operation.data_length());
  return true;
}

bool DeltaPerformer::PerformMoveOperation(
    const DeltaArchiveManifest_InstallOperation& operation) {
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
    const DeltaArchiveManifest_InstallOperation& operation) {
  // Since we delete data off the beginning of the buffer as we use it,
  // the data we need should be exactly at the beginning of the buffer.
  CHECK_EQ(buffer_offset_, operation.data_offset());
  CHECK_GE(buffer_.size(), operation.data_length());

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
    ScopedFdCloser fd_closer(&fd);
    TEST_AND_RETURN_FALSE(
        utils::WriteAll(fd, &buffer_[0], operation.data_length()));
  }
  vector<string> cmd;
  cmd.push_back(kBspatchPath);
  cmd.push_back(path_);
  cmd.push_back(path_);
  cmd.push_back(temp_filename);
  cmd.push_back(input_positions);
  cmd.push_back(output_positions);
  int return_code = 0;
  TEST_AND_RETURN_FALSE(Subprocess::SynchronousExec(cmd, &return_code));
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

  // Update buffer.
  buffer_offset_ += operation.data_length();
  RemoveBufferHeadBytes(&buffer_, operation.data_length());
  return true;
}

}  // namespace chromeos_update_engine
