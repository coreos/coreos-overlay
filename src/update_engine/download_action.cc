// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/download_action.h"
#include <errno.h>
#include <algorithm>
#include <string>
#include <vector>
#include <glib.h>
#include "update_engine/action_pipe.h"
#include "update_engine/subprocess.h"

using std::min;
using std::string;
using std::vector;

namespace chromeos_update_engine {

// Use a buffer to reduce the number of IOPS on SSD devices.
const size_t kFileWriterBufferSize = 128 * 1024;  // 128 KiB

DownloadAction::DownloadAction(PrefsInterface* prefs,
                               SystemState* system_state,
                               HttpFetcher* http_fetcher)
    : prefs_(prefs),
      system_state_(system_state),
      http_fetcher_(http_fetcher),
      writer_(NULL),
      code_(kActionCodeSuccess),
      delegate_(NULL),
      bytes_received_(0) {}

DownloadAction::~DownloadAction() {}

void DownloadAction::PerformAction() {
  http_fetcher_->set_delegate(this);

  // Get the InstallPlan and read it
  CHECK(HasInputObject());
  install_plan_ = GetInputObject();
  bytes_received_ = 0;

  install_plan_.Dump();

  if (writer_) {
    LOG(INFO) << "Using writer for test.";
  } else {
    delta_performer_.reset(new DeltaPerformer(prefs_,
                                              system_state_,
                                              &install_plan_));
    writer_ = delta_performer_.get();
  }
  int rc = writer_->Open(install_plan_.install_path.c_str(),
                         O_TRUNC | O_WRONLY | O_CREAT | O_LARGEFILE,
                         0644);
  if (rc < 0) {
    LOG(ERROR) << "Unable to open output file " << install_plan_.install_path;
    // report error to processor
    processor_->ActionComplete(this, kActionCodeInstallDeviceOpenError);
    return;
  }

  if (delegate_) {
    delegate_->SetDownloadStatus(true);  // Set to active.
  }
  http_fetcher_->BeginTransfer(install_plan_.download_url);
}

void DownloadAction::TerminateProcessing() {
  if (writer_) {
    LOG_IF(WARNING, writer_->Close() != 0) << "Error closing the writer.";
    writer_ = NULL;
  }
  if (delegate_) {
    delegate_->SetDownloadStatus(false);  // Set to inactive.
  }
  // Terminates the transfer. The action is terminated, if necessary, when the
  // TransferTerminated callback is received.
  http_fetcher_->TerminateTransfer();
}

void DownloadAction::SeekToOffset(off_t offset) {
  bytes_received_ = offset;
}

void DownloadAction::ReceivedBytes(HttpFetcher *fetcher,
                                   const char* bytes,
                                   int length) {
  bytes_received_ += length;
  if (delegate_)
    delegate_->BytesReceived(bytes_received_, install_plan_.payload_size);
  if (writer_ && !writer_->Write(bytes, length, &code_)) {
    LOG(ERROR) << "Error " << code_ << " in DeltaPerformer's Write method when "
               << "processing the received payload -- Terminating processing";
    // Don't tell the action processor that the action is complete until we get
    // the TransferTerminated callback. Otherwise, this and the HTTP fetcher
    // objects may get destroyed before all callbacks are complete.
    TerminateProcessing();
    return;
  }
}

void DownloadAction::TransferComplete(HttpFetcher *fetcher, bool successful) {
  if (writer_) {
    LOG_IF(WARNING, writer_->Close() != 0) << "Error closing the writer.";
    writer_ = NULL;
  }
  if (delegate_) {
    delegate_->SetDownloadStatus(false);  // Set to inactive.
  }
  ActionExitCode code =
      successful ? kActionCodeSuccess : kActionCodeDownloadTransferError;
  if (code == kActionCodeSuccess && delta_performer_.get()) {
    code = delta_performer_->VerifyPayload(install_plan_.payload_hash,
                                           install_plan_.payload_size);
    if (code != kActionCodeSuccess) {
      LOG(ERROR) << "Download of " << install_plan_.download_url
                 << " failed due to payload verification error.";
    } else if (!delta_performer_->GetNewPartitionInfo(
        &install_plan_.kernel_size,
        &install_plan_.kernel_hash,
        &install_plan_.rootfs_size,
        &install_plan_.rootfs_hash)) {
      LOG(ERROR) << "Unable to get new partition hash info.";
      code = kActionCodeDownloadNewPartitionInfoError;
    }
  }

  // Write the path to the output pipe if we're successful.
  if (code == kActionCodeSuccess && HasOutputPipe())
    SetOutputObject(install_plan_);
  processor_->ActionComplete(this, code);
}

void DownloadAction::TransferTerminated(HttpFetcher *fetcher) {
  if (code_ != kActionCodeSuccess) {
    processor_->ActionComplete(this, code_);
  }
}

};  // namespace {}
