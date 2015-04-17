// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/stringprintf.h>

#include "update_engine/multi_range_http_fetcher.h"
#include "update_engine/utils.h"

namespace chromeos_update_engine {

// Begins the transfer to the specified URL.
// State change: Stopped -> Downloading
// (corner case: Stopped -> Stopped for an empty request)
void MultiRangeHttpFetcher::BeginTransfer(const std::string& url) {
  CHECK(!base_fetcher_active_) << "BeginTransfer but already active.";
  CHECK(!pending_transfer_ended_) << "BeginTransfer but pending.";
  CHECK(!terminating_) << "BeginTransfer but terminating.";

  if (ranges_.empty()) {
    // Note that after the callback returns this object may be destroyed.
    if (delegate_)
      delegate_->TransferComplete(this, true);
    return;
  }
  url_ = url;
  current_index_ = 0;
  bytes_received_this_range_ = 0;
  LOG(INFO) << "starting first transfer";
  base_fetcher_->set_delegate(this);
  StartTransfer();
}

// State change: Downloading -> Pending transfer ended
void MultiRangeHttpFetcher::TerminateTransfer() {
  if (!base_fetcher_active_) {
    LOG(INFO) << "Called TerminateTransfer but not active.";
    // Note that after the callback returns this object may be destroyed.
    if (delegate_)
      delegate_->TransferTerminated(this);
    return;
  }
  terminating_ = true;
    
  if (!pending_transfer_ended_) {
    base_fetcher_->TerminateTransfer();
  }
}

// State change: Stopped or Downloading -> Downloading
void MultiRangeHttpFetcher::StartTransfer() {
  if (current_index_ >= ranges_.size()) {
    return;
  }

  Range range = ranges_[current_index_];
  LOG(INFO) << "starting transfer of range " << range.ToString();

  bytes_received_this_range_ = 0;
  base_fetcher_->SetOffset(range.offset());
  if (range.HasLength())
    base_fetcher_->SetLength(range.length());
  else
    base_fetcher_->UnsetLength();
  if (delegate_)
    delegate_->SeekToOffset(range.offset());
  base_fetcher_active_ = true;
  base_fetcher_->BeginTransfer(url_);
}

// State change: Downloading -> Downloading or Pending transfer ended
void MultiRangeHttpFetcher::ReceivedBytes(HttpFetcher* fetcher,
                                          const char* bytes,
                                          int length) {
  CHECK_LT(current_index_, ranges_.size());
  CHECK_EQ(fetcher, base_fetcher_.get());
  CHECK(!pending_transfer_ended_);
  size_t next_size = length;
  Range range = ranges_[current_index_];
  if (range.HasLength()) {
    next_size = std::min(next_size,
                         range.length() - bytes_received_this_range_);
  }
  LOG_IF(WARNING, next_size <= 0) << "Asked to write length <= 0";
  if (delegate_) {
    delegate_->ReceivedBytes(this, bytes, next_size);
  }
  bytes_received_this_range_ += length;
  if (range.HasLength() && bytes_received_this_range_ >= range.length()) {
    // Terminates the current fetcher. Waits for its TransferTerminated
    // callback before starting the next range so that we don't end up
    // signalling the delegate that the whole multi-transfer is complete
    // before all fetchers are really done and cleaned up.
    pending_transfer_ended_ = true;
    LOG(INFO) << "terminating transfer";
    fetcher->TerminateTransfer();
  }
}

// State change: Downloading or Pending transfer ended -> Stopped
void MultiRangeHttpFetcher::TransferEnded(HttpFetcher* fetcher,
                                          bool successful) {
  CHECK(base_fetcher_active_) << "Transfer ended unexpectedly.";
  CHECK_EQ(fetcher, base_fetcher_.get());
  pending_transfer_ended_ = false;
  http_response_code_ = fetcher->http_response_code();
  LOG(INFO) << "TransferEnded w/ code " << http_response_code_;
  if (terminating_) {
    LOG(INFO) << "Terminating.";
    Reset();
    // Note that after the callback returns this object may be destroyed.
    if (delegate_)
      delegate_->TransferTerminated(this);
    return;
  }

  // If we didn't get enough bytes, it's failure
  Range range = ranges_[current_index_];
  if (range.HasLength()) {
    if (bytes_received_this_range_ < range.length()) {
      // Failure
      LOG(INFO) << "Didn't get enough bytes. Ending w/ failure.";
      Reset();
      // Note that after the callback returns this object may be destroyed.
      if (delegate_)
        delegate_->TransferComplete(this, false);
      return;
    }
    // We got enough bytes and there were bytes specified, so this is success.
    successful = true;
  }

  // If we have another transfer, do that.
  if (current_index_ + 1 < ranges_.size()) {
    current_index_++;
    LOG(INFO) << "Starting next transfer (" << current_index_ << ").";
    StartTransfer();
    return;
  }

  LOG(INFO) << "Done w/ all transfers";
  Reset();
  // Note that after the callback returns this object may be destroyed.
  if (delegate_)
    delegate_->TransferComplete(this, successful);
}

void MultiRangeHttpFetcher::TransferComplete(HttpFetcher* fetcher,
                                             bool successful) {
  LOG(INFO) << "Received transfer complete.";
  TransferEnded(fetcher, successful);
}

void MultiRangeHttpFetcher::TransferTerminated(HttpFetcher* fetcher) {
  LOG(INFO) << "Received transfer terminated.";
  TransferEnded(fetcher, false);
}

void MultiRangeHttpFetcher::Reset() {
  base_fetcher_active_ = pending_transfer_ended_ = terminating_ = false;
  current_index_ = 0;
  bytes_received_this_range_ = 0;
}

std::string MultiRangeHttpFetcher::Range::ToString() const {
  std::string range_str = StringPrintf("%jd+", offset());
  if (HasLength())
    base::StringAppendF(&range_str, "%zu", length());
  else
    base::StringAppendF(&range_str, "?");
  return range_str;
}

}  // namespace chromeos_update_engine
