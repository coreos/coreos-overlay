// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glib.h>

#include "update_engine/action_pipe.h"
#include "update_engine/download_action.h"

namespace chromeos_update_engine {

DownloadAction::DownloadAction(const std::string& url,
                               const std::string& output_path,
                               off_t size, const std::string& hash,
                               const bool should_decompress,
                               HttpFetcher* http_fetcher)
    : size_(size),
      url_(url),
      output_path_(output_path),
      hash_(hash),
      should_decompress_(should_decompress),
      writer_(NULL),
      http_fetcher_(http_fetcher) {}

DownloadAction::~DownloadAction() {}

void DownloadAction::PerformAction() {
  http_fetcher_->set_delegate(this);
  CHECK(!writer_);
  direct_file_writer_.reset(new DirectFileWriter);

  if (should_decompress_) {
    decompressing_file_writer_.reset(
        new GzipDecompressingFileWriter(direct_file_writer_.get()));
    writer_ = decompressing_file_writer_.get();
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
  int bytes_written = 0;
  do {
    CHECK_GT(length, bytes_written);
    int rc = writer_->Write(bytes + bytes_written, length - bytes_written);
    // TODO(adlr): handle write error
    CHECK_GE(rc, 0);
    bytes_written += rc;
  } while (length != bytes_written);
  omaha_hash_calculator_.Update(bytes, length);
}

void DownloadAction::TransferComplete(HttpFetcher *fetcher, bool successful) {
  if (writer_) {
    CHECK_EQ(0, writer_->Close()) << errno;
    writer_ = NULL;
  }
  if (successful) {
    // Make sure hash is correct
    omaha_hash_calculator_.Finalize();
    if (omaha_hash_calculator_.hash() != hash_) {
      LOG(INFO) << "Download of " << url_ << " failed. Expect hash "
                << hash_ << " but got hash " << omaha_hash_calculator_.hash();
      successful = false;
    }
  }

  // Write the path to the output pipe if we're successful
  if (successful && HasOutputPipe())
    SetOutputObject(output_path_);
  processor_->ActionComplete(this, successful);
}

};  // namespace {}
