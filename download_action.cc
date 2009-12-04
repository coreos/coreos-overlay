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
    : size_(0),
      should_decompress_(false),
      writer_(NULL),
      http_fetcher_(http_fetcher) {}

DownloadAction::~DownloadAction() {}

void DownloadAction::PerformAction() {
  http_fetcher_->set_delegate(this);
  CHECK(!writer_);
  direct_file_writer_.reset(new DirectFileWriter);

  // Get the InstallPlan and read it
  CHECK(HasInputObject());
  InstallPlan install_plan(GetInputObject());

  should_decompress_ = install_plan.is_full_update;
  url_ = install_plan.download_url;
  output_path_ = install_plan.download_path;
  hash_ = install_plan.download_hash;
  install_plan.Dump();

  if (should_decompress_) {
    decompressing_file_writer_.reset(
        new GzipDecompressingFileWriter(direct_file_writer_.get()));
    writer_ = decompressing_file_writer_.get();
    output_path_ = install_plan.install_path;
  } else {
    writer_ = direct_file_writer_.get();
  }
  int rc = writer_->Open(output_path_.c_str(),
                         O_TRUNC | O_WRONLY | O_CREAT | O_LARGEFILE, 0644);
  if (rc < 0) {
    LOG(ERROR) << "Unable to open output file " << output_path_;
    // report error to processor
    processor_->ActionComplete(this, false);
    return;
  }
  http_fetcher_->BeginTransfer(url_);
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
    if (omaha_hash_calculator_.hash() != hash_) {
      LOG(ERROR) << "Download of " << url_ << " failed. Expect hash "
                 << hash_ << " but got hash " << omaha_hash_calculator_.hash();
      successful = false;
    }
  }

  // Write the path to the output pipe if we're successful
  if (successful && HasOutputPipe())
    SetOutputObject(GetInputObject());
  processor_->ActionComplete(this, successful);
}

};  // namespace {}
