// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/download_action.h"
#include <errno.h>
#include <algorithm>
#include <glib.h>
#include "update_engine/action_pipe.h"

using std::min;

namespace chromeos_update_engine {

DownloadAction::DownloadAction(HttpFetcher* http_fetcher)
    : writer_(NULL),
      http_fetcher_(http_fetcher) {}

DownloadAction::~DownloadAction() {}

void DownloadAction::PerformAction() {
  http_fetcher_->set_delegate(this);

  // Get the InstallPlan and read it
  CHECK(HasInputObject());
  install_plan_ = GetInputObject();

  install_plan_.Dump();

  if (writer_) {
    LOG(INFO) << "Using writer for test.";
  } else {
    if (install_plan_.is_full_update) {
      kernel_file_writer_.reset(new DirectFileWriter);
      rootfs_file_writer_.reset(new DirectFileWriter);
      split_file_writer_.reset(new SplitFileWriter(kernel_file_writer_.get(),
                                                   rootfs_file_writer_.get()));
      split_file_writer_->SetFirstOpenArgs(
          install_plan_.kernel_install_path.c_str(),
          O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE,
          0644);
      decompressing_file_writer_.reset(
          new GzipDecompressingFileWriter(split_file_writer_.get()));
      writer_ = decompressing_file_writer_.get();
    } else {
      delta_performer_.reset(new DeltaPerformer);
      writer_ = delta_performer_.get();
    }
  }
  int rc = writer_->Open(install_plan_.install_path.c_str(),
                         O_TRUNC | O_WRONLY | O_CREAT | O_LARGEFILE,
                         0644);
  if (rc < 0) {
    LOG(ERROR) << "Unable to open output file " << install_plan_.install_path;
    // report error to processor
    processor_->ActionComplete(this, false);
    return;
  }
  if (!install_plan_.is_full_update) {
    if (!delta_performer_->OpenKernel(
            install_plan_.kernel_install_path.c_str())) {
      LOG(ERROR) << "Unable to open kernel file "
                 << install_plan_.kernel_install_path.c_str();
      writer_->Close();
      processor_->ActionComplete(this, false);
      return;
    }
  }
  http_fetcher_->BeginTransfer(install_plan_.download_url);
}

void DownloadAction::TerminateProcessing() {
  CHECK(writer_);
  CHECK_EQ(writer_->Close(), 0);
  writer_ = NULL;
  http_fetcher_->TerminateTransfer();
}

void DownloadAction::ReceivedBytes(HttpFetcher *fetcher,
                                   const char* bytes,
                                   int length) {
  int rc = writer_->Write(bytes, length);
  TEST_AND_RETURN(rc >= 0);
  omaha_hash_calculator_.Update(bytes, length);
}

void DownloadAction::TransferComplete(HttpFetcher *fetcher, bool successful) {
  if (writer_) {
    CHECK_EQ(writer_->Close(), 0) << errno;
    writer_ = NULL;
  }
  if (successful) {
    // Make sure hash is correct
    omaha_hash_calculator_.Finalize();
    if (omaha_hash_calculator_.hash() != install_plan_.download_hash) {
      LOG(ERROR) << "Download of " << install_plan_.download_url
                 << " failed. Expect hash " << install_plan_.download_hash
                 << " but got hash " << omaha_hash_calculator_.hash();
      successful = false;
    }
  }

  // Write the path to the output pipe if we're successful
  if (successful && HasOutputPipe())
    SetOutputObject(GetInputObject());
  processor_->ActionComplete(this, successful);
}

};  // namespace {}
