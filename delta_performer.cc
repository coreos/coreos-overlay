// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/delta_performer.h"

#include <endian.h>
#include <errno.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <google/protobuf/repeated_field.h>

#include "update_engine/bzip_extent_writer.h"
#include "update_engine/delta_diff_generator.h"
#include "update_engine/extent_ranges.h"
#include "update_engine/extent_writer.h"
#include "update_engine/graph_types.h"
#include "update_engine/payload_signer.h"
#include "update_engine/prefs_interface.h"
#include "update_engine/subprocess.h"
#include "update_engine/terminator.h"

using std::min;
using std::string;
using std::vector;
using google::protobuf::RepeatedPtrField;

namespace chromeos_update_engine {

const char DeltaPerformer::kUpdatePayloadPublicKeyPath[] =
    "/usr/share/update_engine/update-payload-key.pub.pem";

namespace {

const int kDeltaVersionLength = 8;
const int kDeltaProtobufLengthLength = 8;
const int kUpdateStateOperationInvalid = -1;
const int kMaxResumedUpdateFailures = 10;

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
  LOG(INFO) << "  block_size: " << manifest.block_size();
  for (int i = 0; i < (manifest.install_operations_size() +
                       manifest.kernel_install_operations_size()); i++) {
    const DeltaArchiveManifest_InstallOperation& op =
        i < manifest.install_operations_size() ?
        manifest.install_operations(i) :
        manifest.kernel_install_operations(
            i - manifest.install_operations_size());
    if (i == 0)
      LOG(INFO) << "  Rootfs ops:";
    else if (i == manifest.install_operations_size())
      LOG(INFO) << "   Kernel ops:";
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
    const DeltaArchiveManifest_InstallOperation& op) {
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

int DeltaPerformer::Open(const char* path, int flags, mode_t mode) {
  int err;
  if (OpenFile(path, &fd_, &err))
    path_ = path;
  return -err;
}

bool DeltaPerformer::OpenKernel(const char* kernel_path) {
  int err;
  bool success = OpenFile(kernel_path, &kernel_fd_, &err);
  if (success)
    kernel_path_ = kernel_path;
  return success;
}

int DeltaPerformer::Close() {
  int err = 0;
  if (close(kernel_fd_) == -1) {
    err = errno;
    PLOG(ERROR) << "Unable to close kernel fd:";
  }
  if (close(fd_) == -1) {
    err = errno;
    PLOG(ERROR) << "Unable to close rootfs fd:";
  }
  LOG_IF(ERROR, !hash_calculator_.Finalize()) << "Unable to finalize the hash.";
  fd_ = -2;  // Set to invalid so that calls to Open() will fail.
  path_ = "";
  if (!buffer_.empty()) {
    LOG(ERROR) << "Called Close() while buffer not empty!";
    if (err >= 0) {
      err = 1;
    }
  }
  return -err;
}

namespace {

void LogPartitionInfoHash(const PartitionInfo& info, const string& tag) {
  string sha256;
  if (OmahaHashCalculator::Base64Encode(info.hash().data(),
                                        info.hash().size(),
                                        &sha256)) {
    LOG(INFO) << "PartitionInfo " << tag << " sha256: " << sha256
              << " size: " << info.size();
  } else {
    LOG(ERROR) << "Base64Encode failed for tag: " << tag;
  }
}

void LogPartitionInfo(const DeltaArchiveManifest& manifest) {
  if (manifest.has_old_kernel_info())
    LogPartitionInfoHash(manifest.old_kernel_info(), "old_kernel_info");
  if (manifest.has_old_rootfs_info())
    LogPartitionInfoHash(manifest.old_rootfs_info(), "old_rootfs_info");
  if (manifest.has_new_kernel_info())
    LogPartitionInfoHash(manifest.new_kernel_info(), "new_kernel_info");
  if (manifest.has_new_rootfs_info())
    LogPartitionInfoHash(manifest.new_rootfs_info(), "new_rootfs_info");
}

}  // namespace {}

DeltaPerformer::MetadataParseResult DeltaPerformer::ParsePayloadMetadata(
    const std::vector<char>& payload,
    DeltaArchiveManifest* manifest,
    uint64_t* metadata_size,
    ActionExitCode* error) {
  *error = kActionCodeSuccess;

  const uint64_t protobuf_offset = strlen(kDeltaMagic) + kDeltaVersionLength +
                                   kDeltaProtobufLengthLength;
  if (payload.size() < protobuf_offset) {
    // Don't have enough bytes to know the protobuf length.
    return kMetadataParseInsufficientData;
  }

  // Validate the magic number.
  if (memcmp(payload.data(), kDeltaMagic, strlen(kDeltaMagic)) != 0) {
    LOG(ERROR) << "Bad payload format -- invalid delta magic.";
    *error = kActionCodeDownloadInvalidManifest;
    return kMetadataParseError;
  }

  // TODO(jaysri): Compare the version number and skip unknown manifest
  // versions. We don't check the version at all today.

  // Next, parse the proto buf length.
  uint64_t protobuf_length;
  COMPILE_ASSERT(sizeof(protobuf_length) == kDeltaProtobufLengthLength,
                 protobuf_length_size_mismatch);
  memcpy(&protobuf_length,
         &payload[strlen(kDeltaMagic) + kDeltaVersionLength],
         kDeltaProtobufLengthLength);
  protobuf_length = be64toh(protobuf_length);  // switch big endian to host

  // Now, check if the metasize we computed matches what was passed in
  // through Omaha Response.
  *metadata_size = protobuf_offset + protobuf_length;

  // If the manifest size is present in install plan, check for it immediately
  // even before waiting for that many number of bytes to be downloaded
  // in the payload. This will prevent any attack which relies on us downloading
  // data beyond the expected manifest size.
  if (install_plan_->manifest_size > 0 &&
      install_plan_->manifest_size != *metadata_size) {
      LOG(ERROR) << "Invalid manifest size. Expected = "
                 << install_plan_->manifest_size
                 << "Actual = " << *metadata_size;
      // TODO(jaysri): Add a UMA Stat here to help with the decision to enforce
      // this check in a future release, as mentioned below.

      // TODO(jaysri): VALIDATION: Initially we don't want to make this a fatal
      // error.  But in the next release, we should uncomment the lines below.
      // *error = kActionCodeDownloadInvalidManifest;
      // return kMetadataParseError;
  }

  // Now that we have validated the metadata size, we should wait for the full
  // metadata to be read in before we can parse it.
  if (payload.size() < *metadata_size) {
    return kMetadataParseInsufficientData;
  }

  // Log whether we validated the size or simply trusting what's in the payload
  // here. This is logged here (after we received the full manifest data) so
  // that we just log once (instead of logging n times) if it takes n
  // DeltaPerformer::Write calls to download the full manifest.
  if (install_plan_->manifest_size == 0)
    LOG(WARNING) << "No manifest size specified in Omaha. "
                 << "Trusting manifest size in payload = " << *metadata_size;
  else
    LOG(INFO) << "Manifest size in payload matches expected value from Omaha";

  // We have the full proto buffer in |payload|. Verify its integrity
  // and authenticity based on what Omaha says.
  *error = ValidateManifestSignature(&payload[protobuf_offset],
                                     protobuf_length);
  if (*error != kActionCodeSuccess) {
    // TODO(jaysri): Add a UMA Stat here to help with the decision to enforce
    // this check in a future release, as mentioned below.

    // TODO(jaysri): VALIDATION: Initially we don't want to make this a fatal
    // error.  But in the next release, we should remove the line below and
    // return an error.
    *error = kActionCodeSuccess;
  }

  // The proto buffer in |payload| is deemed valid. Parse it.
  if (!manifest->ParseFromArray(&payload[protobuf_offset], protobuf_length)) {
    LOG(ERROR) << "Unable to parse manifest in update file.";
    *error = kActionCodeDownloadManifestParseError;
    return kMetadataParseError;
  }
  return kMetadataParseSuccess;
}


// Wrapper around write. Returns true if all requested bytes
// were written, or false on any error, reguardless of progress
// and stores an action exit code in |error|.
bool DeltaPerformer::Write(const void* bytes, size_t count,
                           ActionExitCode *error) {
  *error = kActionCodeSuccess;

  const char* c_bytes = reinterpret_cast<const char*>(bytes);
  buffer_.insert(buffer_.end(), c_bytes, c_bytes + count);
  if (!manifest_valid_) {
    MetadataParseResult result = ParsePayloadMetadata(buffer_,
                                                      &manifest_,
                                                      &manifest_metadata_size_,
                                                      error);
    if (result == kMetadataParseError) {
      return false;
    }
    if (result == kMetadataParseInsufficientData) {
      return true;
    }
    // Remove protobuf and header info from buffer_, so buffer_ contains
    // just data blobs
    DiscardBufferHeadBytes(manifest_metadata_size_);
    LOG_IF(WARNING, !prefs_->SetInt64(kPrefsManifestMetadataSize,
                                      manifest_metadata_size_))
        << "Unable to save the manifest metadata size.";
    manifest_valid_ = true;

    LogPartitionInfo(manifest_);
    if (!PrimeUpdateState()) {
      *error = kActionCodeDownloadStateInitializationError;
      LOG(ERROR) << "Unable to prime the update state.";
      return false;
    }
  }
  ssize_t total_operations = manifest_.install_operations_size() +
      manifest_.kernel_install_operations_size();
  while (next_operation_num_ < total_operations) {
    const DeltaArchiveManifest_InstallOperation &op =
        next_operation_num_ < manifest_.install_operations_size() ?
        manifest_.install_operations(next_operation_num_) :
        manifest_.kernel_install_operations(
            next_operation_num_ - manifest_.install_operations_size());
    if (!CanPerformInstallOperation(op)) {
      // This means we don't have enough bytes received yet to carry out the
      // next operation.
      return true;
    }

    *error = ValidateOperationHash(op);
    if (*error != kActionCodeSuccess) {
      // Cannot proceed further as operation hash is invalid.
      // Higher level code will take care of retrying appropriately.
      return false;
    }

    // Makes sure we unblock exit when this operation completes.
    ScopedTerminatorExitUnblocker exit_unblocker =
        ScopedTerminatorExitUnblocker();  // Avoids a compiler unused var bug.
    // Log every thousandth operation, and also the first and last ones
    if ((next_operation_num_ % 1000 == 0) ||
        (next_operation_num_ + 1 == total_operations)) {
      LOG(INFO) << "Performing operation " << (next_operation_num_ + 1) << "/"
                << total_operations;
    }
    bool is_kernel_partition =
        (next_operation_num_ >= manifest_.install_operations_size());
    if (op.type() == DeltaArchiveManifest_InstallOperation_Type_REPLACE ||
        op.type() == DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ) {
      if (!PerformReplaceOperation(op, is_kernel_partition)) {
        LOG(ERROR) << "Failed to perform replace operation "
                   << next_operation_num_;
        *error = kActionCodeDownloadOperationExecutionError;
        return false;
      }
    } else if (op.type() == DeltaArchiveManifest_InstallOperation_Type_MOVE) {
      if (!PerformMoveOperation(op, is_kernel_partition)) {
        LOG(ERROR) << "Failed to perform move operation "
                   << next_operation_num_;
        *error = kActionCodeDownloadOperationExecutionError;
        return false;
      }
    } else if (op.type() == DeltaArchiveManifest_InstallOperation_Type_BSDIFF) {
      if (!PerformBsdiffOperation(op, is_kernel_partition)) {
        LOG(ERROR) << "Failed to perform bsdiff operation "
                   << next_operation_num_;
        *error = kActionCodeDownloadOperationExecutionError;
        return false;
      }
    }
    next_operation_num_++;
    CheckpointUpdateProgress();
  }
  return true;
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
    const DeltaArchiveManifest_InstallOperation& operation,
    bool is_kernel_partition) {
  CHECK(operation.type() == \
        DeltaArchiveManifest_InstallOperation_Type_REPLACE || \
        operation.type() == \
        DeltaArchiveManifest_InstallOperation_Type_REPLACE_BZ);

  // Since we delete data off the beginning of the buffer as we use it,
  // the data we need should be exactly at the beginning of the buffer.
  TEST_AND_RETURN_FALSE(buffer_offset_ == operation.data_offset());
  TEST_AND_RETURN_FALSE(buffer_.size() >= operation.data_length());

  // Extract the signature message if it's in this operation.
  ExtractSignatureMessage(operation);

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

  int fd = is_kernel_partition ? kernel_fd_ : fd_;

  TEST_AND_RETURN_FALSE(writer->Init(fd, extents, block_size_));
  TEST_AND_RETURN_FALSE(writer->Write(&buffer_[0], operation.data_length()));
  TEST_AND_RETURN_FALSE(writer->End());

  // Update buffer
  buffer_offset_ += operation.data_length();
  DiscardBufferHeadBytes(operation.data_length());
  return true;
}

bool DeltaPerformer::PerformMoveOperation(
    const DeltaArchiveManifest_InstallOperation& operation,
    bool is_kernel_partition) {
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

  int fd = is_kernel_partition ? kernel_fd_ : fd_;

  // Read in bytes.
  ssize_t bytes_read = 0;
  for (int i = 0; i < operation.src_extents_size(); i++) {
    ssize_t bytes_read_this_iteration = 0;
    const Extent& extent = operation.src_extents(i);
    TEST_AND_RETURN_FALSE(utils::PReadAll(fd,
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
    ResetUpdateProgress(prefs_, true);
  }

  // Write bytes out.
  ssize_t bytes_written = 0;
  for (int i = 0; i < operation.dst_extents_size(); i++) {
    const Extent& extent = operation.dst_extents(i);
    TEST_AND_RETURN_FALSE(utils::PWriteAll(fd,
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
    const DeltaArchiveManifest_InstallOperation& operation,
    bool is_kernel_partition) {
  // Since we delete data off the beginning of the buffer as we use it,
  // the data we need should be exactly at the beginning of the buffer.
  TEST_AND_RETURN_FALSE(buffer_offset_ == operation.data_offset());
  TEST_AND_RETURN_FALSE(buffer_.size() >= operation.data_length());

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

  int fd = is_kernel_partition ? kernel_fd_ : fd_;
  const string& path = StringPrintf("/dev/fd/%d", fd);

  // If this is a non-idempotent operation, request a delayed exit and clear the
  // update state in case the operation gets interrupted. Do this as late as
  // possible.
  if (!IsIdempotentOperation(operation)) {
    Terminator::set_exit_blocked(true);
    ResetUpdateProgress(prefs_, true);
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
        utils::PWriteAll(fd, &zeros[0], end_byte - begin_byte, begin_byte));
  }

  // Update buffer.
  buffer_offset_ += operation.data_length();
  DiscardBufferHeadBytes(operation.data_length());
  return true;
}

bool DeltaPerformer::ExtractSignatureMessage(
    const DeltaArchiveManifest_InstallOperation& operation) {
  if (operation.type() != DeltaArchiveManifest_InstallOperation_Type_REPLACE ||
      !manifest_.has_signatures_offset() ||
      manifest_.signatures_offset() != operation.data_offset()) {
    return false;
  }
  TEST_AND_RETURN_FALSE(manifest_.has_signatures_size() &&
                        manifest_.signatures_size() == operation.data_length());
  TEST_AND_RETURN_FALSE(signatures_message_data_.empty());
  TEST_AND_RETURN_FALSE(buffer_offset_ == manifest_.signatures_offset());
  TEST_AND_RETURN_FALSE(buffer_.size() >= manifest_.signatures_size());
  signatures_message_data_.assign(
      buffer_.begin(),
      buffer_.begin() + manifest_.signatures_size());

  // Save the signature blob because if the update is interrupted after the
  // download phase we don't go through this path anymore. Some alternatives to
  // consider:
  //
  // 1. On resume, re-download the signature blob from the server and re-verify
  // it.
  //
  // 2. Verify the signature as soon as it's received and don't checkpoint the
  // blob and the signed sha-256 context.
  LOG_IF(WARNING, !prefs_->SetString(kPrefsUpdateStateSignatureBlob,
                                     string(&signatures_message_data_[0],
                                            signatures_message_data_.size())))
      << "Unable to store the signature blob.";
  // The hash of all data consumed so far should be verified against the signed
  // hash.
  signed_hash_context_ = hash_calculator_.GetContext();
  LOG_IF(WARNING, !prefs_->SetString(kPrefsUpdateStateSignedSHA256Context,
                                     signed_hash_context_))
      << "Unable to store the signed hash context.";
  LOG(INFO) << "Extracted signature data of size "
            << manifest_.signatures_size() << " at "
            << manifest_.signatures_offset();
  return true;
}

ActionExitCode DeltaPerformer::ValidateManifestSignature(
    const char* protobuf, uint64_t protobuf_length) {

  if (install_plan_->manifest_signature.empty()) {
    // If this is not present, we cannot validate the manifest. This should
    // never happen in normal circumstances, but this can be used as a
    // release-knob to turn off the new code path that verify per-operation
    // hashes. So, for now, we should not treat this as a failure. Once we are
    // confident this path is bug-free, we should treat this as a failure so
    // that we remain robust even if the connection to Omaha is subjected to
    // any SSL attack.
    LOG(WARNING) << "Cannot validate manifest as the signature is empty";
    return kActionCodeSuccess;
  }

  // Convert base64-encoded signature to raw bytes.
  vector<char> manifest_signature;
  if (!OmahaHashCalculator::Base64Decode(install_plan_->manifest_signature,
                                         &manifest_signature)) {
    LOG(ERROR) << "Unable to decode base64 manifest signature: "
               << install_plan_->manifest_signature;
    return kActionCodeDownloadManifestSignatureError;
  }

  vector<char> expected_manifest_hash;
  if (!PayloadSigner::GetRawHashFromSignature(manifest_signature,
                                              public_key_path_,
                                              &expected_manifest_hash)) {
    LOG(ERROR) << "Unable to compute expected hash from manifest signature";
    return kActionCodeDownloadManifestSignatureError;
  }

  OmahaHashCalculator manifest_hasher;
  manifest_hasher.Update(protobuf, protobuf_length);
  if (!manifest_hasher.Finalize()) {
    LOG(ERROR) << "Unable to compute actual hash of manifest";
    return kActionCodeDownloadManifestSignatureVerificationError;
  }

  vector<char> calculated_manifest_hash = manifest_hasher.raw_hash();
  PayloadSigner::PadRSA2048SHA256Hash(&calculated_manifest_hash);
  if (calculated_manifest_hash.empty()) {
    LOG(ERROR) << "Computed actual hash of manifest is empty.";
    return kActionCodeDownloadManifestSignatureVerificationError;
  }

  if (calculated_manifest_hash != expected_manifest_hash) {
    LOG(ERROR) << "Manifest hash verification failed. Expected hash = ";
    utils::HexDumpVector(expected_manifest_hash);
    LOG(ERROR) << "Calculated hash = ";
    utils::HexDumpVector(calculated_manifest_hash);
    return kActionCodeDownloadManifestSignatureMismatch;
  }

  LOG(INFO) << "Manifest signature matches expected value in Omaha response";
  return kActionCodeSuccess;
}

ActionExitCode DeltaPerformer::ValidateOperationHash(
    const DeltaArchiveManifest_InstallOperation&
    operation) {

  // TODO(jaysri): To be implemented.
  return kActionCodeSuccess;
}

#define TEST_AND_RETURN_VAL(_retval, _condition)                \
  do {                                                          \
    if (!(_condition)) {                                        \
      LOG(ERROR) << "VerifyPayload failure: " << #_condition;   \
      return _retval;                                           \
    }                                                           \
  } while (0);

ActionExitCode DeltaPerformer::VerifyPayload(
    const std::string& update_check_response_hash,
    const uint64_t update_check_response_size) {
  LOG(INFO) << "Verifying delta payload using public key: " << public_key_path_;

  // Verifies the download size.
  TEST_AND_RETURN_VAL(kActionCodePayloadSizeMismatchError,
                      update_check_response_size ==
                      manifest_metadata_size_ + buffer_offset_);

  // Verifies the payload hash.
  const string& payload_hash_data = hash_calculator_.hash();
  TEST_AND_RETURN_VAL(kActionCodeDownloadPayloadVerificationError,
                      !payload_hash_data.empty());
  TEST_AND_RETURN_VAL(kActionCodePayloadHashMismatchError,
                      payload_hash_data == update_check_response_hash);

  // Verifies the signed payload hash.
  if (!utils::FileExists(public_key_path_.c_str())) {
    LOG(WARNING) << "Not verifying signed delta payload -- missing public key.";
    return kActionCodeSuccess;
  }
  TEST_AND_RETURN_VAL(kActionCodeSignedDeltaPayloadExpectedError,
                      !signatures_message_data_.empty());
  vector<char> signed_hash_data;
  TEST_AND_RETURN_VAL(kActionCodeDownloadPayloadPubKeyVerificationError,
                      PayloadSigner::VerifySignature(
                          signatures_message_data_,
                          public_key_path_,
                          &signed_hash_data));
  OmahaHashCalculator signed_hasher;
  TEST_AND_RETURN_VAL(kActionCodeDownloadPayloadPubKeyVerificationError,
                      signed_hasher.SetContext(signed_hash_context_));
  TEST_AND_RETURN_VAL(kActionCodeDownloadPayloadPubKeyVerificationError,
                      signed_hasher.Finalize());
  vector<char> hash_data = signed_hasher.raw_hash();
  PayloadSigner::PadRSA2048SHA256Hash(&hash_data);
  TEST_AND_RETURN_VAL(kActionCodeDownloadPayloadPubKeyVerificationError,
                      !hash_data.empty());
  if (hash_data != signed_hash_data) {
    LOG(ERROR) << "Public key verification failed, thus update failed. "
        "Attached Signature:";
    utils::HexDumpVector(signed_hash_data);
    LOG(ERROR) << "Computed Signature:";
    utils::HexDumpVector(hash_data);
    return kActionCodeDownloadPayloadPubKeyVerificationError;
  }
  return kActionCodeSuccess;
}

bool DeltaPerformer::GetNewPartitionInfo(uint64_t* kernel_size,
                                         vector<char>* kernel_hash,
                                         uint64_t* rootfs_size,
                                         vector<char>* rootfs_hash) {
  TEST_AND_RETURN_FALSE(manifest_valid_ &&
                        manifest_.has_new_kernel_info() &&
                        manifest_.has_new_rootfs_info());
  *kernel_size = manifest_.new_kernel_info().size();
  *rootfs_size = manifest_.new_rootfs_info().size();
  vector<char> new_kernel_hash(manifest_.new_kernel_info().hash().begin(),
                               manifest_.new_kernel_info().hash().end());
  vector<char> new_rootfs_hash(manifest_.new_rootfs_info().hash().begin(),
                               manifest_.new_rootfs_info().hash().end());
  kernel_hash->swap(new_kernel_hash);
  rootfs_hash->swap(new_rootfs_hash);
  return true;
}

namespace {
void LogVerifyError(bool is_kern,
                    const string& local_hash,
                    const string& expected_hash) {
  const char* type = is_kern ? "kernel" : "rootfs";
  LOG(ERROR) << "This is a server-side error due to "
             << "mismatched delta update image!";
  LOG(ERROR) << "The delta I've been given contains a " << type << " delta "
             << "update that must be applied over a " << type << " with "
             << "a specific checksum, but the " << type << " we're starting "
             << "with doesn't have that checksum! This means that "
             << "the delta I've been given doesn't match my existing "
             << "system. The " << type << " partition I have has hash: "
             << local_hash << " but the update expected me to have "
             << expected_hash << " .";
  if (is_kern) {
    LOG(INFO) << "To get the checksum of a kernel partition on a "
              << "booted machine, run this command (change /dev/sda2 "
              << "as needed): dd if=/dev/sda2 bs=1M 2>/dev/null | "
              << "openssl dgst -sha256 -binary | openssl base64";
  } else {
    LOG(INFO) << "To get the checksum of a rootfs partition on a "
              << "booted machine, run this command (change /dev/sda3 "
              << "as needed): dd if=/dev/sda3 bs=1M count=$(( "
              << "$(dumpe2fs /dev/sda3  2>/dev/null | grep 'Block count' "
              << "| sed 's/[^0-9]*//') / 256 )) | "
              << "openssl dgst -sha256 -binary | openssl base64";
  }
  LOG(INFO) << "To get the checksum of partitions in a bin file, "
            << "run: .../src/scripts/sha256_partitions.sh .../file.bin";
}

string StringForHashBytes(const void* bytes, size_t size) {
  string ret;
  if (!OmahaHashCalculator::Base64Encode(bytes, size, &ret)) {
    ret = "<unknown>";
  }
  return ret;
}
}  // namespace

bool DeltaPerformer::VerifySourcePartitions() {
  LOG(INFO) << "Verifying source partitions.";
  CHECK(manifest_valid_);
  CHECK(install_plan_);
  if (manifest_.has_old_kernel_info()) {
    const PartitionInfo& info = manifest_.old_kernel_info();
    bool valid =
        !install_plan_->kernel_hash.empty() &&
        install_plan_->kernel_hash.size() == info.hash().size() &&
        memcmp(install_plan_->kernel_hash.data(),
               info.hash().data(),
               install_plan_->kernel_hash.size()) == 0;
    if (!valid) {
      LogVerifyError(true,
                     StringForHashBytes(install_plan_->kernel_hash.data(),
                                        install_plan_->kernel_hash.size()),
                     StringForHashBytes(info.hash().data(),
                                        info.hash().size()));
    }
    TEST_AND_RETURN_FALSE(valid);
  }
  if (manifest_.has_old_rootfs_info()) {
    const PartitionInfo& info = manifest_.old_rootfs_info();
    bool valid =
        !install_plan_->rootfs_hash.empty() &&
        install_plan_->rootfs_hash.size() == info.hash().size() &&
        memcmp(install_plan_->rootfs_hash.data(),
               info.hash().data(),
               install_plan_->rootfs_hash.size()) == 0;
    if (!valid) {
      LogVerifyError(false,
                     StringForHashBytes(install_plan_->kernel_hash.data(),
                                        install_plan_->kernel_hash.size()),
                     StringForHashBytes(info.hash().data(),
                                        info.hash().size()));
    }
    TEST_AND_RETURN_FALSE(valid);
  }
  return true;
}

void DeltaPerformer::DiscardBufferHeadBytes(size_t count) {
  hash_calculator_.Update(&buffer_[0], count);
  buffer_.erase(buffer_.begin(), buffer_.begin() + count);
}

bool DeltaPerformer::CanResumeUpdate(PrefsInterface* prefs,
                                     string update_check_response_hash) {
  int64_t next_operation = kUpdateStateOperationInvalid;
  TEST_AND_RETURN_FALSE(prefs->GetInt64(kPrefsUpdateStateNextOperation,
                                        &next_operation) &&
                        next_operation != kUpdateStateOperationInvalid &&
                        next_operation > 0);

  string interrupted_hash;
  TEST_AND_RETURN_FALSE(prefs->GetString(kPrefsUpdateCheckResponseHash,
                                         &interrupted_hash) &&
                        !interrupted_hash.empty() &&
                        interrupted_hash == update_check_response_hash);

  int64_t resumed_update_failures;
  TEST_AND_RETURN_FALSE(!prefs->GetInt64(kPrefsResumedUpdateFailures,
                                         &resumed_update_failures) ||
                        resumed_update_failures <= kMaxResumedUpdateFailures);

  // Sanity check the rest.
  int64_t next_data_offset = -1;
  TEST_AND_RETURN_FALSE(prefs->GetInt64(kPrefsUpdateStateNextDataOffset,
                                        &next_data_offset) &&
                        next_data_offset >= 0);

  string sha256_context;
  TEST_AND_RETURN_FALSE(
      prefs->GetString(kPrefsUpdateStateSHA256Context, &sha256_context) &&
      !sha256_context.empty());

  int64_t manifest_metadata_size = 0;
  TEST_AND_RETURN_FALSE(prefs->GetInt64(kPrefsManifestMetadataSize,
                                        &manifest_metadata_size) &&
                        manifest_metadata_size > 0);

  return true;
}

bool DeltaPerformer::ResetUpdateProgress(PrefsInterface* prefs, bool quick) {
  TEST_AND_RETURN_FALSE(prefs->SetInt64(kPrefsUpdateStateNextOperation,
                                        kUpdateStateOperationInvalid));
  if (!quick) {
    prefs->SetString(kPrefsUpdateCheckResponseHash, "");
    prefs->SetInt64(kPrefsUpdateStateNextDataOffset, -1);
    prefs->SetString(kPrefsUpdateStateSHA256Context, "");
    prefs->SetString(kPrefsUpdateStateSignedSHA256Context, "");
    prefs->SetString(kPrefsUpdateStateSignatureBlob, "");
    prefs->SetInt64(kPrefsManifestMetadataSize, -1);
    prefs->SetInt64(kPrefsResumedUpdateFailures, 0);
  }
  return true;
}

bool DeltaPerformer::CheckpointUpdateProgress() {
  Terminator::set_exit_blocked(true);
  if (last_updated_buffer_offset_ != buffer_offset_) {
    // Resets the progress in case we die in the middle of the state update.
    ResetUpdateProgress(prefs_, true);
    TEST_AND_RETURN_FALSE(
        prefs_->SetString(kPrefsUpdateStateSHA256Context,
                          hash_calculator_.GetContext()));
    TEST_AND_RETURN_FALSE(prefs_->SetInt64(kPrefsUpdateStateNextDataOffset,
                                           buffer_offset_));
    last_updated_buffer_offset_ = buffer_offset_;
  }
  TEST_AND_RETURN_FALSE(prefs_->SetInt64(kPrefsUpdateStateNextOperation,
                                         next_operation_num_));
  return true;
}

bool DeltaPerformer::PrimeUpdateState() {
  CHECK(manifest_valid_);
  block_size_ = manifest_.block_size();

  int64_t next_operation = kUpdateStateOperationInvalid;
  if (!prefs_->GetInt64(kPrefsUpdateStateNextOperation, &next_operation) ||
      next_operation == kUpdateStateOperationInvalid ||
      next_operation <= 0) {
    // Initiating a new update, no more state needs to be initialized.
    TEST_AND_RETURN_FALSE(VerifySourcePartitions());
    return true;
  }
  next_operation_num_ = next_operation;

  // Resuming an update -- load the rest of the update state.
  int64_t next_data_offset = -1;
  TEST_AND_RETURN_FALSE(prefs_->GetInt64(kPrefsUpdateStateNextDataOffset,
                                         &next_data_offset) &&
                        next_data_offset >= 0);
  buffer_offset_ = next_data_offset;

  // The signed hash context and the signature blob may be empty if the
  // interrupted update didn't reach the signature.
  prefs_->GetString(kPrefsUpdateStateSignedSHA256Context,
                    &signed_hash_context_);
  string signature_blob;
  if (prefs_->GetString(kPrefsUpdateStateSignatureBlob, &signature_blob)) {
    signatures_message_data_.assign(signature_blob.begin(),
                                    signature_blob.end());
  }

  string hash_context;
  TEST_AND_RETURN_FALSE(prefs_->GetString(kPrefsUpdateStateSHA256Context,
                                          &hash_context) &&
                        hash_calculator_.SetContext(hash_context));

  int64_t manifest_metadata_size = 0;
  TEST_AND_RETURN_FALSE(prefs_->GetInt64(kPrefsManifestMetadataSize,
                                         &manifest_metadata_size) &&
                        manifest_metadata_size > 0);
  manifest_metadata_size_ = manifest_metadata_size;

  // Speculatively count the resume as a failure.
  int64_t resumed_update_failures;
  if (prefs_->GetInt64(kPrefsResumedUpdateFailures, &resumed_update_failures)) {
    resumed_update_failures++;
  } else {
    resumed_update_failures = 1;
  }
  prefs_->SetInt64(kPrefsResumedUpdateFailures, resumed_update_failures);
  return true;
}

}  // namespace chromeos_update_engine
