// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_processor.h"

#include <endian.h>

#include <string>
#include <vector>

#include <google/protobuf/repeated_field.h>

#include "strings/string_printf.h"
#include "update_engine/delta_metadata.h"
#include "update_engine/payload_signer.h"
#include "update_engine/prefs_interface.h"
#include "update_engine/terminator.h"

using std::string;
using std::vector;
using google::protobuf::RepeatedPtrField;
using strings::StringPrintf;

namespace chromeos_update_engine {

const char PayloadProcessor::kUpdatePayloadPublicKeyPath[] =
    "/usr/share/update_engine/update-payload-key.pub.pem";

namespace {
const int kUpdateStateOperationInvalid = -1;
const int kMaxResumedUpdateFailures = 10;

void LogPartitionInfoHash(const InstallInfo& info, const string& tag) {
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

PayloadProcessor::PayloadProcessor(PrefsInterface* prefs, InstallPlan* install_plan)
  : partition_performer_(prefs, install_plan->partition_path),
    kernel_performer_(prefs, install_plan->kernel_path),
    prefs_(prefs),
    install_plan_(install_plan),
    manifest_valid_(false),
    manifest_metadata_size_(0),
    next_operation_num_(0),
    buffer_offset_(0),
    last_updated_buffer_offset_(std::numeric_limits<uint64_t>::max()),
    public_key_path_(kUpdatePayloadPublicKeyPath) {
}

int PayloadProcessor::Open() {
  int err = partition_performer_.Open();
  if (err != 0) {
    return err;
  }
  if (!install_plan_->kernel_path.empty()) {
    err = kernel_performer_.Open();
    if (err != 0) {
      partition_performer_.Close();
      return err;
    }
  }
  return 0;
}

int PayloadProcessor::Close() {
  int err = partition_performer_.Close();
  if (!install_plan_->kernel_path.empty()) {
    int err2 = kernel_performer_.Close();
    if (err == 0) {
      err = err2;
    }
  }
  LOG_IF(ERROR, !hash_calculator_.Finalize()) << "Unable to finalize the hash.";
  if (!buffer_.empty()) {
    LOG(ERROR) << "Called Close() while buffer not empty!";
    if (err == 0) {
      err = -EBUSY;
    }
  }
  if (next_operation_num_ != operations_.size()) {
    LOG(ERROR) << "Called Close() before completing operation "
               << next_operation_num_ << "/" << operations_.size();
    if (err == 0) {
      err = -EBUSY;
    }
  }
  return err;
}

// Wrapper around write. Returns true if all requested bytes
// were written, or false on any error, regardless of progress
// and stores an action exit code in |error|.
bool PayloadProcessor::Write(const void* bytes, size_t count,
                             ActionExitCode *error) {
  *error = kActionCodeSuccess;

  const char* c_bytes = reinterpret_cast<const char*>(bytes);
  buffer_.insert(buffer_.end(), c_bytes, c_bytes + count);

  if (!manifest_valid_) {
    *error = LoadManifest();
    if (*error == kActionCodeDownloadIncomplete) {
      *error = kActionCodeSuccess;
      return true;
    } else if (*error != kActionCodeSuccess)
      return false;
  }

  while (next_operation_num_ < operations_.size()) {
    *error = PerformOperation();
    if (*error == kActionCodeDownloadIncomplete) {
      *error = kActionCodeSuccess;
      return true;
    } else if (*error != kActionCodeSuccess) {
      return false;
    }
  }

  // Make sure the operations consumed exactly the right amount of data.
  if (manifest_.has_signatures_offset() &&
      manifest_.signatures_offset() != buffer_offset_) {
    LOG(ERROR) << "Signatures located at an unexpected data offset "
               << manifest_.signatures_offset()
               << ", expected " << buffer_offset_;
    *error = kActionCodeDownloadOperationExecutionError;
    return false;
  }

  // Any issues with the signature will be reported by VerifyPayload.
  if (ExtractSignatureMessage(buffer_)) {
    buffer_offset_ += manifest_.signatures_size();
    DiscardBufferHeadBytes(manifest_.signatures_size());
  }

  return true;
}

ActionExitCode PayloadProcessor::LoadManifest() {
  DCHECK(!manifest_valid_);

  ActionExitCode error = DeltaMetadata::ParsePayload(
      buffer_, &manifest_, &manifest_metadata_size_);
  if (error != kActionCodeSuccess)
    return error;

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
    return kActionCodeDownloadStateInitializationError;
  }

  partition_performer_.SetBlockSize(manifest_.block_size());
  for (const InstallOperation &op : manifest_.partition_operations()) {
    operations_.emplace_back(nullptr, &op);
  }

  kernel_performer_.SetBlockSize(manifest_.block_size());
  for (const InstallProcedure &proc : manifest_.procedures()) {
    // KERNELs are plain files so we must specify their final size.
    if (proc.has_type() &&
        proc.type() == InstallProcedure_Type_KERNEL &&
        proc.has_new_info() &&
        !install_plan_->kernel_path.empty()) {
      kernel_performer_.SetFileSize(proc.new_info().size());
    }

    for (const InstallOperation &op : proc.operations()) {
      operations_.emplace_back(&proc, &op);
    }
  }

  if (next_operation_num_ > 0)
    LOG(INFO) << "Resuming after " << next_operation_num_ << " operations";
  LOG(INFO) << "Starting to apply update payload operations";

  return kActionCodeSuccess;
}

ActionExitCode PayloadProcessor::PerformOperation() {
  DCHECK(next_operation_num_ < operations_.size());

  const InstallProcedure *proc = operations_[next_operation_num_].first;
  const InstallOperation *op = operations_[next_operation_num_].second;
  DeltaPerformer *performer = nullptr;

  // There is no InstallProcedure type for partitions, so it will be null.
  if (proc == nullptr) {
    performer = &partition_performer_;
  } else if (proc->has_type()) {
    // has_type is only true if it is present and has a known enum value.
    switch (proc->type()) {
      case InstallProcedure_Type_KERNEL:
        if (!install_plan_->kernel_path.empty())
          performer = &kernel_performer_;
        break;
    }
  }

  if (op->data_length() && op->data_offset() != buffer_offset_) {
    LOG(ERROR) << "Operation " << next_operation_num_
               << " skipped to unexpected data offset " << op->data_offset()
               << ", expected " << buffer_offset_;
    return kActionCodeDownloadOperationExecutionError;
  }

  if (op->data_length() > buffer_.size())
    return kActionCodeDownloadIncomplete;

  // Makes sure we unblock exit when this operation completes.
  ScopedTerminatorExitUnblocker exit_unblocker =
      ScopedTerminatorExitUnblocker();  // Avoids a compiler unused var bug.

  if (performer != nullptr) {
    ActionExitCode error = performer->PerformOperation(*op, buffer_);
    if (error != kActionCodeSuccess) {
      LOG(ERROR) << "Aborting install procedure at operation "
                 << next_operation_num_;
      return error;
    }
  }

  buffer_offset_ += op->data_length();
  DiscardBufferHeadBytes(op->data_length());
  next_operation_num_++;

  LOG(INFO) << (performer?"Completed ":"Skipped ")
            << next_operation_num_ << "/"
            << operations_.size() << " operations ("
            << (next_operation_num_ * 100 / operations_.size()) << "%)";
  CheckpointUpdateProgress();

  return kActionCodeSuccess;
}

bool PayloadProcessor::ExtractSignatureMessage(const vector<char>& data) {
  TEST_AND_RETURN_FALSE(manifest_.has_signatures_offset());
  TEST_AND_RETURN_FALSE(manifest_.has_signatures_size());
  TEST_AND_RETURN_FALSE(signatures_message_data_.empty());
  TEST_AND_RETURN_FALSE(data.size() >= manifest_.signatures_size());
  signatures_message_data_.assign(
      data.begin(),
      data.begin() + manifest_.signatures_size());

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

#define TEST_AND_RETURN_VAL(_retval, _condition)                \
  do {                                                          \
    if (!(_condition)) {                                        \
      LOG(ERROR) << "VerifyPayload failure: " << #_condition;   \
      return _retval;                                           \
    }                                                           \
  } while (0);

ActionExitCode PayloadProcessor::VerifyPayload() {
  LOG(INFO) << "Verifying delta payload using public key: " << public_key_path_;

  // Verifies the download size.
  TEST_AND_RETURN_VAL(kActionCodePayloadSizeMismatchError,
                      install_plan_->payload_size ==
                      manifest_metadata_size_ + buffer_offset_);

  // Verifies the payload hash.
  const string& payload_hash_data = hash_calculator_.hash();
  TEST_AND_RETURN_VAL(kActionCodeDownloadPayloadVerificationError,
                      !payload_hash_data.empty());
  TEST_AND_RETURN_VAL(kActionCodePayloadHashMismatchError,
                      payload_hash_data == install_plan_->payload_hash);

  // Update the InstallPlan so the update can be verified.
  TEST_AND_RETURN_VAL(kActionCodeDownloadPayloadVerificationError,
                      SetNewPartitionInfo());
  TEST_AND_RETURN_VAL(kActionCodeDownloadPayloadVerificationError,
                      SetNewKernelInfo());

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

bool PayloadProcessor::SetNewPartitionInfo() {
  TEST_AND_RETURN_FALSE(manifest_valid_ &&
                        manifest_.has_new_partition_info());
  install_plan_->new_partition_size = manifest_.new_partition_info().size();
  install_plan_->new_partition_hash.assign(
      manifest_.new_partition_info().hash().begin(),
      manifest_.new_partition_info().hash().end());
  return true;
}

bool PayloadProcessor::SetNewKernelInfo() {
  if (install_plan_->kernel_path.empty())
    return true;

  for (const InstallProcedure& proc : manifest_.procedures()) {
    if (!proc.has_type() || proc.type() != InstallProcedure_Type_KERNEL)
      continue;

    TEST_AND_RETURN_FALSE(proc.has_new_info());
    install_plan_->new_kernel_size = proc.new_info().size();
    install_plan_->new_kernel_hash.assign(
        proc.new_info().hash().begin(),
        proc.new_info().hash().end());
    return true;
  }

  // Allow payloads without kernels, the verify action will be skipped
  // if the kernel size and hash are unset.
  return true;
}

namespace {
string StringForHashVector(const vector<char>& hash) {
  string ret;
  if (!OmahaHashCalculator::Base64Encode(hash.data(), hash.size(), &ret)) {
    ret = "<unknown>";
  }
  return ret;
}

void LogVerifyError(const vector<char>& local_hash_data,
                    const vector<char>& expected_hash_data) {
  string local_hash = StringForHashVector(local_hash_data);
  string expected_hash = StringForHashVector(expected_hash_data);
  LOG(ERROR) << "This is either disk corruption or server-side error "
             << "due to a mismatched delta update image!";
  LOG(ERROR) << "The delta I've been given contains a partition delta "
             << "update that must be applied over a partition with "
             << "a specific checksum, but the partition we're starting "
             << "with doesn't have that checksum! This means that "
             << "the delta I've been given doesn't match my existing "
             << "system. The partition I have has hash: "
             << local_hash << " but the update expected me to have "
             << expected_hash << " .";
}
}  // namespace

bool PayloadProcessor::VerifySourcePartition() {
  LOG(INFO) << "Verifying source partition.";
  CHECK(manifest_valid_);
  CHECK(install_plan_);
  if (manifest_.has_old_partition_info()) {
    CHECK(!install_plan_->old_partition_hash.empty());
    const InstallInfo& info = manifest_.old_partition_info();
    const vector<char> hash(info.hash().begin(), info.hash().end());
    if (install_plan_->old_partition_hash != hash) {
      LogVerifyError(install_plan_->old_partition_hash, hash);
      return false;
    }
  }
  return true;
}

void PayloadProcessor::DiscardBufferHeadBytes(size_t count) {
  hash_calculator_.Update(&buffer_[0], count);
  buffer_.erase(buffer_.begin(), buffer_.begin() + count);
}

bool PayloadProcessor::CanResumeUpdate(PrefsInterface* prefs,
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

bool PayloadProcessor::ResetUpdateProgress(PrefsInterface* prefs, bool quick) {
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

bool PayloadProcessor::CheckpointUpdateProgress() {
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

bool PayloadProcessor::PrimeUpdateState() {
  CHECK(manifest_valid_);

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
