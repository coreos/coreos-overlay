// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/mock_http_fetcher.h"

#include <algorithm>

#include <glog/logging.h>
#include <gtest/gtest.h>

// This is a mock implementation of HttpFetcher which is useful for testing.

using std::min;

namespace chromeos_update_engine {

MockHttpFetcher::~MockHttpFetcher() {
  CHECK(!timeout_source_) << "Call TerminateTransfer() before dtor.";
}

void MockHttpFetcher::BeginTransfer(const std::string& url) {
  EXPECT_FALSE(never_use_);
  if (fail_transfer_ || data_.empty()) {
    // No data to send, just notify of completion..
    SignalTransferComplete();
    return;
  }
  if (sent_size_ < data_.size())
    SendData(true);
}

// Returns false on one condition: If timeout_source_ was already set
// and it needs to be deleted by the caller. If timeout_source_ is NULL
// when this function is called, this function will always return true.
bool MockHttpFetcher::SendData(bool skip_delivery) {
  if (fail_transfer_) {
    SignalTransferComplete();
    return timeout_source_;
  }

  CHECK_LT(sent_size_, data_.size());
  if (!skip_delivery) {
    const size_t chunk_size = min(kMockHttpFetcherChunkSize,
                                  data_.size() - sent_size_);
    CHECK(delegate_);
    delegate_->ReceivedBytes(this, &data_[sent_size_], chunk_size);
    // We may get terminated in the callback.
    if (sent_size_ == data_.size()) {
      LOG(INFO) << "Terminated in the ReceivedBytes callback.";
      return timeout_source_;
    }
    sent_size_ += chunk_size;
    CHECK_LE(sent_size_, data_.size());
    if (sent_size_ == data_.size()) {
      // We've sent all the data. Notify of success.
      SignalTransferComplete();
    }
  }

  if (paused_) {
    // If we're paused, we should return true if timeout_source_ is set,
    // since we need the caller to delete it.
    return timeout_source_;
  }

  if (timeout_source_) {
    // we still need a timeout if there's more data to send
    return sent_size_ < data_.size();
  } else if (sent_size_ < data_.size()) {
    // we don't have a timeout source and we need one
    timeout_source_ = g_timeout_source_new(10);
    CHECK(timeout_source_);
    g_source_set_callback(timeout_source_, StaticTimeoutCallback, this,
                          NULL);
    timout_tag_ = g_source_attach(timeout_source_, NULL);
  }
  return true;
}

bool MockHttpFetcher::TimeoutCallback() {
  CHECK(!paused_);
  bool ret = SendData(false);
  if (false == ret) {
    timeout_source_ = NULL;
  }
  return ret;
}

// If the transfer is in progress, aborts the transfer early.
// The transfer cannot be resumed.
void MockHttpFetcher::TerminateTransfer() {
  LOG(INFO) << "Terminating transfer.";
  sent_size_ = data_.size();
  // kill any timeout
  if (timeout_source_) {
    g_source_remove(timout_tag_);
    g_source_destroy(timeout_source_);
    timeout_source_ = NULL;
  }
  delegate_->TransferTerminated(this);
}

void MockHttpFetcher::Pause() {
  CHECK(!paused_);
  paused_ = true;
  if (timeout_source_) {
    g_source_remove(timout_tag_);
    g_source_destroy(timeout_source_);
    timeout_source_ = NULL;
  }

}

void MockHttpFetcher::Unpause() {
  CHECK(paused_) << "You must pause before unpause.";
  paused_ = false;
  if (sent_size_ < data_.size()) {
    SendData(false);
  }
}

void MockHttpFetcher::FailTransfer(int http_response_code) {
  fail_transfer_ = true;
  http_response_code_ = http_response_code;
}

void MockHttpFetcher::SignalTransferComplete() {
  // If the transfer has been failed, the HTTP response code should be set
  // already.
  if (!fail_transfer_) {
    http_response_code_ = 200;
  }
  delegate_->TransferComplete(this, !fail_transfer_);
}

}  // namespace chromeos_update_engine
