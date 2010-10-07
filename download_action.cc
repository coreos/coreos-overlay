// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
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
                               HttpFetcher* http_fetcher)
    : prefs_(prefs),
      writer_(NULL),
      http_fetcher_(http_fetcher),
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
    if (install_plan_.is_full_update) {
      kernel_file_writer_.reset(new DirectFileWriter);
      rootfs_file_writer_.reset(new DirectFileWriter);
      kernel_buffered_file_writer_.reset(
          new BufferedFileWriter(kernel_file_writer_.get(),
                                 kFileWriterBufferSize));
      rootfs_buffered_file_writer_.reset(
          new BufferedFileWriter(rootfs_file_writer_.get(),
                                 kFileWriterBufferSize));
      split_file_writer_.reset(
          new SplitFileWriter(kernel_buffered_file_writer_.get(),
                              rootfs_buffered_file_writer_.get()));
      split_file_writer_->SetFirstOpenArgs(
          install_plan_.kernel_install_path.c_str(),
          O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE,
          0644);
      decompressing_file_writer_.reset(
          new GzipDecompressingFileWriter(split_file_writer_.get()));
      writer_ = decompressing_file_writer_.get();
    } else {
      delta_performer_.reset(new DeltaPerformer(prefs_));
      writer_ = delta_performer_.get();
    }
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
  if (!install_plan_.is_full_update) {
    if (!delta_performer_->OpenKernel(
            install_plan_.kernel_install_path.c_str())) {
      LOG(ERROR) << "Unable to open kernel file "
                 << install_plan_.kernel_install_path.c_str();
      writer_->Close();
      processor_->ActionComplete(this, kActionCodeKernelDeviceOpenError);
      return;
    }
  }
  if (delegate_) {
    delegate_->SetDownloadStatus(true);  // Set to active.
  }
  http_fetcher_->BeginTransfer(install_plan_.download_url);
}

void DownloadAction::TerminateProcessing() {
  CHECK(writer_);
  CHECK_EQ(writer_->Close(), 0);
  writer_ = NULL;
  http_fetcher_->TerminateTransfer();
  if (delegate_) {
    delegate_->SetDownloadStatus(false);  // Set to inactive.
  }
}

void DownloadAction::ReceivedBytes(HttpFetcher *fetcher,
                                   const char* bytes,
                                   int length) {
  bytes_received_ += length;
  if (delegate_)
    delegate_->BytesReceived(bytes_received_, install_plan_.size);
  int rc = writer_->Write(bytes, length);
  TEST_AND_RETURN(rc >= 0);
  omaha_hash_calculator_.Update(bytes, length);
}

namespace {
void FlushLinuxCaches() {
  vector<string> command;
  command.push_back("/bin/sync");
  int rc;
  LOG(INFO) << "FlushLinuxCaches-sync...";
  Subprocess::SynchronousExec(command, &rc);
  LOG(INFO) << "FlushLinuxCaches-drop_caches...";

  const char* const drop_cmd = "3\n";
  utils::WriteFile("/proc/sys/vm/drop_caches", drop_cmd, strlen(drop_cmd));

  LOG(INFO) << "FlushLinuxCaches done.";
}
}

void DownloadAction::TransferComplete(HttpFetcher *fetcher, bool successful) {
  if (writer_) {
    CHECK_EQ(writer_->Close(), 0) << errno;
    writer_ = NULL;
  }
  if (delegate_) {
    delegate_->SetDownloadStatus(false);  // Set to inactive.
  }
  ActionExitCode code =
      successful ? kActionCodeSuccess : kActionCodeDownloadTransferError;
  if (code == kActionCodeSuccess) {
    if (!install_plan_.is_full_update) {
      if (!delta_performer_->VerifyPayload("",
                                           install_plan_.download_hash,
                                           install_plan_.size)) {
        LOG(ERROR) << "Download of " << install_plan_.download_url
                   << " failed due to payload verification error.";
        code = kActionCodeDownloadPayloadVerificationError;
      }
    } else {
      // Makes sure the hash and size are correct for an old-style full update.
      omaha_hash_calculator_.Finalize();
      if (omaha_hash_calculator_.hash() != install_plan_.download_hash) {
        LOG(ERROR) << "Download of " << install_plan_.download_url
                   << " failed. Expected hash " << install_plan_.download_hash
                   << " but got hash " << omaha_hash_calculator_.hash();
        code = kActionCodeDownloadHashMismatchError;
      } else if (bytes_received_ != install_plan_.size) {
        LOG(ERROR) << "Download of " << install_plan_.download_url
                   << " failed. Expected size " << install_plan_.size
                   << " but got size " << bytes_received_;
        code = kActionCodeDownloadSizeMismatchError;
      }
    }
  }

  FlushLinuxCaches();

  // Write the path to the output pipe if we're successful.
  if (code == kActionCodeSuccess && HasOutputPipe())
    SetOutputObject(GetInputObject());
  processor_->ActionComplete(this, code);
}

};  // namespace {}
