// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/delta_performer.h"

#include <endian.h>
#include <errno.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <base/string_util.h>
#include <google/protobuf/repeated_field.h>

#include "strings/string_printf.h"
#include "update_engine/bzip_extent_writer.h"
#include "update_engine/delta_metadata.h"
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
using strings::StringPrintf;

namespace chromeos_update_engine {

const char DeltaPerformer::kUpdatePayloadPublicKeyPath[] =
    "/usr/share/update_engine/update-payload-key.pub.pem";

namespace {
const int kUpdateStateOperationInvalid = -1;
const int kMaxResumedUpdateFailures = 10;

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
    PLOG(ERROR) << "Unable to close partition fd:";
  }
  LOG_IF(ERROR, !hash_calculator_.Finalize()) << "Unable to finalize the hash.";
  fd_ = -2;  // Set to invalid so that calls to Open() will fail.
  if (!buffer_.empty()) {
    LOG(ERROR) << "Called Close() while buffer not empty!";
    if (err >= 0) {
      err = 1;
    }
  }
  return -err;
}

namespace {

void LogPartitionInfoHash(const BlobInfo& info, const string& tag) {
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
  if (manifest.has_old_partition_info())
    LogPartitionInfoHash(manifest.old_partition_info(), "old_partition_info");
  if (manifest.has_new_partition_info())
    LogPartitionInfoHash(manifest.new_partition_info(), "new_partition_info");
}

}  // namespace {}


// Wrapper around write. Returns true if all requested bytes
// were written, or false on any error, regardless of progress
// and stores an action exit code in |error|.
bool DeltaPerformer::Write(const void* bytes, size_t count,
                           ActionExitCode *error) {
  *error = kActionCodeSuccess;

  const char* c_bytes = reinterpret_cast<const char*>(bytes);
  buffer_.insert(buffer_.end(), c_bytes, c_bytes + count);

  if (!manifest_valid_) {
    DeltaMetadata::ParseResult result = DeltaMetadata::ParsePayload(
        buffer_, &manifest_, &manifest_metadata_size_, error);
    switch (result) {
      case DeltaMetadata::kParseError: return false;
      case DeltaMetadata::kParseInsufficientData: return true;
      case DeltaMetadata::kParseSuccess: break;
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

    num_rootfs_operations_ = manifest_.partition_operations_size();
    num_total_operations_ =
        num_rootfs_operations_ + manifest_.noop_operations_size();
    if (next_operation_num_ > 0)
      LOG(INFO) << "Resuming after " << next_operation_num_ << " operations";
    LOG(INFO) << "Starting to apply update payload operations";
  }

  while (next_operation_num_ < num_total_operations_) {
    const bool is_noop =
        (next_operation_num_ >= num_rootfs_operations_);
    const InstallOperation &op =
        is_noop ?
        manifest_.noop_operations(
            next_operation_num_ - num_rootfs_operations_) :
        manifest_.partition_operations(next_operation_num_);
    if (!CanPerformInstallOperation(op)) {
      // This means we don't have enough bytes received yet to carry out the
      // next operation.
      return true;
    }

    // Makes sure we unblock exit when this operation completes.
    ScopedTerminatorExitUnblocker exit_unblocker =
        ScopedTerminatorExitUnblocker();  // Avoids a compiler unused var bug.

    *error = PerformOperation(op);
    if (*error != kActionCodeSuccess) {
      LOG(ERROR) << "Aborting install procedure at operation "
                 << num_total_operations_;
      return false;
    }

    next_operation_num_++;
    LOG(INFO) << "Completed " << next_operation_num_ << "/"
              << num_total_operations_ << " operations ("
              << (next_operation_num_ * 100 / num_total_operations_) << "%)";
    CheckpointUpdateProgress();
  }
  return true;
}

ActionExitCode DeltaPerformer::PerformOperation(
    const InstallOperation& operation) {
  // Note: Validate must be called only if CanPerformInstallOperation is
  // called. Otherwise, we might be failing operations before even if there
  // isn't sufficient data to compute the proper hash.
  ActionExitCode error = ValidateOperationHash(operation);
  if (error != kActionCodeSuccess) {
    if (install_plan_->hash_checks_mandatory) {
      LOG(ERROR) << "Mandatory operation hash check failed";
      return error;
    }

    // For non-mandatory cases, just log a warning.
    LOG(WARNING) << "Ignoring operation validation errors";
  }

  // TODO(marineam): Add a PerformNoopOperation and call it here instead
  // of assuming PerformReplaceOperation will safely noop.

  // Log every thousandth operation, and also the first and last ones
  if (operation.type() == InstallOperation_Type_REPLACE ||
      operation.type() == InstallOperation_Type_REPLACE_BZ) {
    if (!PerformReplaceOperation(operation)) {
      LOG(ERROR) << "Failed to perform replace operation";
      return kActionCodeDownloadOperationExecutionError;
    }
  } else if (operation.type() == InstallOperation_Type_MOVE) {
    if (!PerformMoveOperation(operation)) {
      LOG(ERROR) << "Failed to perform move operation";
      return kActionCodeDownloadOperationExecutionError;
    }
  } else if (operation.type() == InstallOperation_Type_BSDIFF) {
    if (!PerformBsdiffOperation(operation)) {
      LOG(ERROR) << "Failed to perform bsdiff operation";
      return kActionCodeDownloadOperationExecutionError;
    }
  } else {
    NOTREACHED();
  }

  return kActionCodeSuccess;
}

bool DeltaPerformer::CanPerformInstallOperation(
    const chromeos_update_engine::InstallOperation&
    operation) {
  // Move operations don't require any data blob, so they can always
  // be performed
  if (operation.type() == InstallOperation_Type_MOVE)
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
    const InstallOperation& operation) {
  CHECK(operation.type() == \
        InstallOperation_Type_REPLACE || \
        operation.type() == \
        InstallOperation_Type_REPLACE_BZ);

  // Since we delete data off the beginning of the buffer as we use it,
  // the data we need should be exactly at the beginning of the buffer.
  TEST_AND_RETURN_FALSE(buffer_offset_ == operation.data_offset());
  TEST_AND_RETURN_FALSE(buffer_.size() >= operation.data_length());

  // Extract the signature message if it's in this operation.
  ExtractSignatureMessage(operation);

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

  TEST_AND_RETURN_FALSE(writer->Init(fd_, extents, block_size_));
  TEST_AND_RETURN_FALSE(writer->Write(&buffer_[0], operation.data_length()));
  TEST_AND_RETURN_FALSE(writer->End());
  // Update buffer
  buffer_offset_ += operation.data_length();
  DiscardBufferHeadBytes(operation.data_length());
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
    ResetUpdateProgress(prefs_, true);
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
    const InstallOperation& operation) {
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

  const string& path = StringPrintf("/dev/fd/%d", fd_);

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
        utils::PWriteAll(fd_, &zeros[0], end_byte - begin_byte, begin_byte));
  }

  // Update buffer.
  buffer_offset_ += operation.data_length();
  DiscardBufferHeadBytes(operation.data_length());
  return true;
}

bool DeltaPerformer::ExtractSignatureMessage(
    const InstallOperation& operation) {
  if (operation.type() != InstallOperation_Type_REPLACE ||
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

ActionExitCode DeltaPerformer::ValidateOperationHash(
    const InstallOperation& operation) {

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
    // One caveat though: The last operation is a dummy signature operation
    // that doesn't have a hash at the time the manifest is created. So we
    // should not complaint about that operation. This operation can be
    // recognized by the fact that it's offset is mentioned in the manifest.
    if (manifest_.signatures_offset() &&
        manifest_.signatures_offset() == operation.data_offset()) {
      LOG(INFO) << "Skipping hash verification for signature operation "
                << next_operation_num_ + 1;
    } else {
      if (install_plan_->hash_checks_mandatory) {
        LOG(ERROR) << "Missing mandatory operation hash for operation "
                   << next_operation_num_ + 1;
        return kActionCodeDownloadOperationHashMissingError;
      }

      // For non-mandatory cases, just log a warning.
      LOG(WARNING) << "Cannot validate operation " << next_operation_num_ + 1
                   << " as there's no operation hash in manifest";
    }
    return kActionCodeSuccess;
  }

  vector<char> expected_op_hash;
  expected_op_hash.assign(operation.data_sha256_hash().data(),
                          (operation.data_sha256_hash().data() +
                           operation.data_sha256_hash().size()));

  OmahaHashCalculator operation_hasher;
  operation_hasher.Update(&buffer_[0], operation.data_length());
  if (!operation_hasher.Finalize()) {
    LOG(ERROR) << "Unable to compute actual hash of operation "
               << next_operation_num_;
    return kActionCodeDownloadOperationHashVerificationError;
  }

  vector<char> calculated_op_hash = operation_hasher.raw_hash();
  if (calculated_op_hash != expected_op_hash) {
    LOG(ERROR) << "Hash verification failed for operation "
               << next_operation_num_ << ". Expected hash = ";
    utils::HexDumpVector(expected_op_hash);
    LOG(ERROR) << "Calculated hash over " << operation.data_length()
               << " bytes at offset: " << operation.data_offset() << " = ";
    utils::HexDumpVector(calculated_op_hash);
    return kActionCodeDownloadOperationHashMismatch;
  }

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

bool DeltaPerformer::GetNewPartitionInfo(uint64_t* rootfs_size,
                                         vector<char>* rootfs_hash) {
  TEST_AND_RETURN_FALSE(manifest_valid_ &&
			manifest_.has_new_partition_info());
  *rootfs_size = manifest_.new_partition_info().size();
  vector<char> new_rootfs_hash(manifest_.new_partition_info().hash().begin(),
                               manifest_.new_partition_info().hash().end());
  rootfs_hash->swap(new_rootfs_hash);
  return true;
}

namespace {
void LogVerifyError(const string& local_hash,
                    const string& expected_hash) {
  LOG(ERROR) << "This is a server-side error due to "
             << "mismatched delta update image!";
  LOG(ERROR) << "The delta I've been given contains a partition delta "
             << "update that must be applied over a partition with "
             << "a specific checksum, but the partition we're starting "
             << "with doesn't have that checksum! This means that "
             << "the delta I've been given doesn't match my existing "
             << "system. The partition I have has hash: "
             << local_hash << " but the update expected me to have "
             << expected_hash << " .";
  LOG(INFO) << "To get the checksum of a partition on a "
            << "booted machine, run this command (change /dev/sda3 "
            << "as needed): dd if=/dev/sda3 bs=1M count=$(( "
            << "$(dumpe2fs /dev/sda3  2>/dev/null | grep 'Block count' "
            << "| sed 's/[^0-9]*//') / 256 )) | "
            << "openssl dgst -sha256 -binary | openssl base64";
}

string StringForHashBytes(const void* bytes, size_t size) {
  string ret;
  if (!OmahaHashCalculator::Base64Encode(bytes, size, &ret)) {
    ret = "<unknown>";
  }
  return ret;
}
}  // namespace

bool DeltaPerformer::VerifySourcePartition() {
  LOG(INFO) << "Verifying source partition.";
  CHECK(manifest_valid_);
  CHECK(install_plan_);
  if (manifest_.has_old_partition_info()) {
    const BlobInfo& info = manifest_.old_partition_info();
    bool valid =
        !install_plan_->rootfs_hash.empty() &&
        install_plan_->rootfs_hash.size() == info.hash().size() &&
        memcmp(install_plan_->rootfs_hash.data(),
               info.hash().data(),
               install_plan_->rootfs_hash.size()) == 0;
    if (!valid) {
      LogVerifyError(StringForHashBytes(install_plan_->rootfs_hash.data(),
                                        install_plan_->rootfs_hash.size()),
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
    TEST_AND_RETURN_FALSE(VerifySourcePartition());
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
