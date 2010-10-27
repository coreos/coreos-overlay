// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_MULTI_HTTP_FETCHER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_MULTI_HTTP_FETCHER_H__

#include <tr1/memory>
#include <utility>
#include <vector>

#include "update_engine/http_fetcher.h"

// This class is a simple wrapper around an HttpFetcher. The client
// specifies a vector of byte ranges. MultiHttpFetcher will fetch bytes
// from those offsets. Pass -1 as a length to specify unlimited length.
// It really only would make sense for the last range specified to have
// unlimited length.

namespace chromeos_update_engine {

template<typename BaseHttpFetcher>
class MultiHttpFetcher : public HttpFetcher, public HttpFetcherDelegate {
 public:
  typedef std::vector<std::pair<off_t, off_t> > RangesVect;

  MultiHttpFetcher()
      : sent_transfer_complete_(false),
        current_index_(0),
        bytes_received_this_fetcher_(0) {}
  ~MultiHttpFetcher() {}

  void set_ranges(const RangesVect& ranges) {
    ranges_ = ranges;
    fetchers_.resize(ranges_.size());  // Allocate the fetchers
    for (typename std::vector<std::tr1::shared_ptr<BaseHttpFetcher>
             >::iterator it = fetchers_.begin(), e = fetchers_.end();
         it != e; ++it) {
      (*it) = std::tr1::shared_ptr<BaseHttpFetcher>(new BaseHttpFetcher);
      (*it)->set_delegate(this);
    }
  }

  void SetOffset(off_t offset) {}  // for now, doesn't support this

  // Begins the transfer to the specified URL.
  void BeginTransfer(const std::string& url) {
    url_ = url;
    if (ranges_.empty()) {
      if (delegate_)
        delegate_->TransferComplete(this, true);
      return;
    }
    current_index_ = 0;
    LOG(INFO) << "starting first transfer";
    StartTransfer();
  }

  void TerminateTransfer() {
    if (current_index_ < fetchers_.size())
      fetchers_[current_index_]->TerminateTransfer();
    current_index_ = ranges_.size();
    sent_transfer_complete_ = true;  // a fib
  }

  void Pause() {
    if (current_index_ < fetchers_.size())
      fetchers_[current_index_]->Pause();
  }

  void Unpause() {
    if (current_index_ < fetchers_.size())
      fetchers_[current_index_]->Unpause();
  }

  // These functions are overloaded in LibcurlHttp fetcher for testing purposes.
  void set_idle_seconds(int seconds) {
    for (typename std::vector<std::tr1::shared_ptr<BaseHttpFetcher> >::iterator
             it = fetchers_.begin(),
             e = fetchers_.end(); it != e; ++it) {
      (*it)->set_idle_seconds(seconds);
    }
  }
  void set_retry_seconds(int seconds) {
    for (typename std::vector<std::tr1::shared_ptr<BaseHttpFetcher> >::iterator
             it = fetchers_.begin(),
             e = fetchers_.end(); it != e; ++it) {
      (*it)->set_retry_seconds(seconds);
    }
  }
  void SetConnectionAsExpensive(bool is_expensive) {
    for (typename std::vector<std::tr1::shared_ptr<BaseHttpFetcher> >::iterator
             it = fetchers_.begin(),
             e = fetchers_.end(); it != e; ++it) {
      (*it)->SetConnectionAsExpensive(is_expensive);
    }
  }
  void SetBuildType(bool is_official) {
    for (typename std::vector<std::tr1::shared_ptr<BaseHttpFetcher> >::iterator
             it = fetchers_.begin(),
             e = fetchers_.end(); it != e; ++it) {
      (*it)->SetBuildType(is_official);
    }
  }

 private:
  void SendTransferComplete(HttpFetcher* fetcher, bool successful) {
    if (sent_transfer_complete_)
      return;
    LOG(INFO) << "Sending transfer complete";
    sent_transfer_complete_ = true;
    http_response_code_ = fetcher->http_response_code();
    if (delegate_)
      delegate_->TransferComplete(this, successful);
  }

  void StartTransfer() {
    if (current_index_ >= ranges_.size()) {
      return;
    }
    LOG(INFO) << "Starting a transfer @" << ranges_[current_index_].first << "("
              << ranges_[current_index_].second << ")";
    bytes_received_this_fetcher_ = 0;
    fetchers_[current_index_]->SetOffset(ranges_[current_index_].first);
    if (delegate_)
      delegate_->SeekToOffset(ranges_[current_index_].first);
    fetchers_[current_index_]->BeginTransfer(url_);
  }

  void ReceivedBytes(HttpFetcher* fetcher,
                     const char* bytes,
                     int length) {
    if (current_index_ >= ranges_.size())
      return;
    if (fetcher != fetchers_[current_index_].get()) {
      LOG(WARNING) << "Received bytes from invalid fetcher";
      return;
    }
    off_t next_size = length;
    if (ranges_[current_index_].second >= 0) {
      next_size = std::min(next_size,
                           ranges_[current_index_].second -
                           bytes_received_this_fetcher_);
    }
    LOG_IF(WARNING, next_size <= 0) << "Asked to write length <= 0";
    if (delegate_) {
      delegate_->ReceivedBytes(this, bytes, next_size);
    }
    bytes_received_this_fetcher_ += length;
    if (ranges_[current_index_].second >= 0 &&
        bytes_received_this_fetcher_ >= ranges_[current_index_].second) {
      fetchers_[current_index_]->TerminateTransfer();
      current_index_++;
      if (current_index_ == ranges_.size()) {
        SendTransferComplete(fetchers_[current_index_ - 1].get(), true);
      } else {
        StartTransfer();
      }
    }
  }

  void TransferComplete(HttpFetcher* fetcher, bool successful) {
    LOG(INFO) << "Received transfer complete";
    if (current_index_ >= ranges_.size()) {
      SendTransferComplete(fetcher, true);
      return;
    }

    if (ranges_[current_index_].second < 0) {
      // We're done with the current operation
      current_index_++;
      if (current_index_ >= ranges_.size() || !successful) {
        SendTransferComplete(fetcher, successful);
      } else {
        // Do the next transfer
        StartTransfer();
      }
      return;
    }

    if (bytes_received_this_fetcher_ < ranges_[current_index_].second) {
      LOG(WARNING) << "Received insufficient bytes from fetcher. "
                   << "Ending early";
      SendTransferComplete(fetcher, false);
      return;
    } else {
      LOG(INFO) << "Got spurious TransferComplete. Ingoring.";
    }
  }

  // If true, do not send any more data or TransferComplete to the delegate.
  bool sent_transfer_complete_;

  RangesVect ranges_;
  std::vector<std::tr1::shared_ptr<BaseHttpFetcher> > fetchers_;

  RangesVect::size_type current_index_;  // index into ranges_, fetchers_
  off_t bytes_received_this_fetcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MultiHttpFetcher);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_MULTI_HTTP_FETCHER_H__
