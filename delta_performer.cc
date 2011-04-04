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

#include <base/scoped_ptr.h>
#include <base/string_util.h>
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
    uint64_t* metadata_size) {
  if (payload.size() < strlen(kDeltaMagic) +
      kDeltaVersionLength + kDeltaProtobufLengthLength) {
    // Don't have enough bytes to know the protobuf length.
    return kMetadataParseInsufficientData;
  }
  if (memcmp(payload.data(), kDeltaMagic, strlen(kDeltaMagic)) != 0) {
    LOG(ERROR) << "Bad payload format -- invalid delta magic.";
    return kMetadataParseError;
  }
  uint64_t protobuf_length;
  COMPILE_ASSERT(sizeof(protobuf_length) == kDeltaProtobufLengthLength,
                 protobuf_length_size_mismatch);
  memcpy(&protobuf_length,
         &payload[strlen(kDeltaMagic) + kDeltaVersionLength],
         kDeltaProtobufLengthLength);
  protobuf_length = be64toh(protobuf_length);  // switch big endian to host
  if (payload.size() < strlen(kDeltaMagic) + kDeltaVersionLength +
      kDeltaProtobufLengthLength + protobuf_length) {
    return kMetadataParseInsufficientData;
  }
  // We have the full proto buffer in |payload|. Parse it.
  const int offset = strlen(kDeltaMagic) + kDeltaVersionLength +
      kDeltaProtobufLengthLength;
  if (!manifest->ParseFromArray(&payload[offset], protobuf_length)) {
    LOG(ERROR) << "Unable to parse manifest in update file.";
    return kMetadataParseError;
  }
  *metadata_size = strlen(kDeltaMagic) + kDeltaVersionLength +
      kDeltaProtobufLengthLength + protobuf_length;
  return kMetadataParseSuccess;
}


// Wrapper around write. Returns bytes written on success or
// -errno on error.
// This function performs as many actions as it can, given the amount of
// data received thus far.
ssize_t DeltaPerformer::Write(const void* bytes, size_t count) {
  const char* c_bytes = reinterpret_cast<const char*>(bytes);
  buffer_.insert(buffer_.end(), c_bytes, c_bytes + count);

  if (!manifest_valid_) {
    MetadataParseResult result = ParsePayloadMetadata(buffer_,
                                                      &manifest_,
                                                      &manifest_metadata_size_);
    if (result == kMetadataParseError) {
      return -EINVAL;
    }
    if (result == kMetadataParseInsufficientData) {
      return count;
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
      LOG(ERROR) << "Unable to prime the update state.";
      return -EINVAL;
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
    if (!CanPerformInstallOperation(op))
      break;
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
        return -EINVAL;
      }
    } else if (op.type() == DeltaArchiveManifest_InstallOperation_Type_MOVE) {
      if (!PerformMoveOperation(op, is_kernel_partition)) {
        LOG(ERROR) << "Failed to perform move operation "
                   << next_operation_num_;
        return -EINVAL;
      }
    } else if (op.type() == DeltaArchiveManifest_InstallOperation_Type_BSDIFF) {
      if (!PerformBsdiffOperation(op, is_kernel_partition)) {
        LOG(ERROR) << "Failed to perform bsdiff operation "
                   << next_operation_num_;
        return -EINVAL;
      }
    }
    next_operation_num_++;
    CheckpointUpdateProgress();
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
                                       &return_code,
                                       G_SPAWN_LEAVE_DESCRIPTORS_OPEN));
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
  signatures_message_data_.insert(
      signatures_message_data_.begin(),
      buffer_.begin(),
      buffer_.begin() + manifest_.signatures_size());
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

#define TEST_SET_TRUE_RET_TRUE(_ptr, _condition)                        \
  do {                                                                  \
    if (!(_condition)) {                                                \
      LOG(ERROR) << "Non fatal public key verification: " << #_condition; \
      if (_ptr) {                                                       \
        *(_ptr) = true;                                                 \
      }                                                                 \
      return true;                                                      \
    }                                                                   \
  } while(0)

bool DeltaPerformer::VerifyPayload(
    const string& public_key_path,
    const std::string& update_check_response_hash,
    const uint64_t update_check_response_size,
    bool* signature_failed) {
  string key_path = public_key_path;
  if (key_path.empty()) {
    key_path = kUpdatePayloadPublicKeyPath;
  }
  LOG(INFO) << "Verifying delta payload. Public key path: " << key_path;

  // Verifies the download hash.
  const string& download_hash_data = hash_calculator_.hash();
  TEST_AND_RETURN_FALSE(!download_hash_data.empty());
  TEST_AND_RETURN_FALSE(download_hash_data == update_check_response_hash);

  // Verifies the download size.
  TEST_AND_RETURN_FALSE(update_check_response_size ==
                        manifest_metadata_size_ + buffer_offset_);

  // Verifies the signed payload hash.
  if (!utils::FileExists(key_path.c_str())) {
    LOG(WARNING) << "Not verifying signed delta payload -- missing public key.";
    return true;
  }
  TEST_SET_TRUE_RET_TRUE(signature_failed, !signatures_message_data_.empty());
  vector<char> signed_hash_data;
  TEST_SET_TRUE_RET_TRUE(signature_failed, PayloadSigner::VerifySignature(
      signatures_message_data_,
      key_path,
      &signed_hash_data));
  OmahaHashCalculator signed_hasher;
  TEST_SET_TRUE_RET_TRUE(signature_failed,
                         signed_hasher.SetContext(signed_hash_context_));
  TEST_SET_TRUE_RET_TRUE(signature_failed,
                         signed_hasher.Finalize());
  vector<char> hash_data = signed_hasher.raw_hash();
  PayloadSigner::PadRSA2048SHA256Hash(&hash_data);
  TEST_SET_TRUE_RET_TRUE(signature_failed, !hash_data.empty());
  if (hash_data != signed_hash_data) {
    LOG(ERROR) << "Public key verificaion failed. This is non-fatal. "
        "Attached Signature:";
    utils::HexDumpVector(signed_hash_data);
    LOG(ERROR) << "Computed Signature:";
    utils::HexDumpVector(hash_data);
    if (signature_failed) {
      *signature_failed = true;
    }
  }
  return true;
}

#undef TEST_SET_TRUE_RET_TRUE

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

bool DeltaPerformer::VerifySourcePartitions() {
  LOG(INFO) << "Verifying source partitions.";
  CHECK(manifest_valid_);
  if (manifest_.has_old_kernel_info()) {
    const PartitionInfo& info = manifest_.old_kernel_info();
    TEST_AND_RETURN_FALSE(!current_kernel_hash_.empty() &&
                          current_kernel_hash_.size() == info.hash().size() &&
                          memcmp(current_kernel_hash_.data(),
                                 info.hash().data(),
                                 current_kernel_hash_.size()) == 0);
  }
  if (manifest_.has_old_rootfs_info()) {
    const PartitionInfo& info = manifest_.old_rootfs_info();
    TEST_AND_RETURN_FALSE(!current_rootfs_hash_.empty() &&
                          current_rootfs_hash_.size() == info.hash().size() &&
                          memcmp(current_rootfs_hash_.data(),
                                 info.hash().data(),
                                 current_rootfs_hash_.size()) == 0);
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

  // The signed hash context may be empty if the interrupted update didn't reach
  // the signature blob.
  prefs_->GetString(kPrefsUpdateStateSignedSHA256Context,
                    &signed_hash_context_);

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
