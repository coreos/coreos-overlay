// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_MULTI_RANGE_HTTP_FETCHER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_MULTI_RANGE_HTTP_FETCHER_H__

#include <deque>
#include <utility>
#include <vector>

#include <base/memory/scoped_ptr.h>

#include "update_engine/http_fetcher.h"

// This class is a simple wrapper around an HttpFetcher. The client
// specifies a vector of byte ranges. MultiRangeHttpFetcher will fetch bytes
// from those offsets, using the same bash fetcher for all ranges. Thus, the
// fetcher must support beginning a transfter after one has stopped. Pass -1
// as a length to specify unlimited length. It really only would make sense
// for the last range specified to have unlimited length, tho it is legal for
// other entries to have unlimited length.

// There are three states a MultiRangeHttpFetcher object will be in:
// - Stopped (start state)
// - Downloading
// - Pending transfer ended
// Various functions below that might change state indicate possible
// state changes.

namespace chromeos_update_engine {

class MultiRangeHttpFetcher : public HttpFetcher, public HttpFetcherDelegate {
 public:
  // Takes ownership of the passed in fetcher.
  explicit MultiRangeHttpFetcher(HttpFetcher* base_fetcher)
      : HttpFetcher(base_fetcher->GetSystemState()),
        base_fetcher_(base_fetcher),
        base_fetcher_active_(false),
        pending_transfer_ended_(false),
        terminating_(false),
        current_index_(0),
        bytes_received_this_range_(0) {}
  ~MultiRangeHttpFetcher() {}

  void ClearRanges() { ranges_.clear(); }

  void AddRange(off_t offset, size_t size) {
    CHECK_GT(size, static_cast<size_t>(0));
    ranges_.push_back(Range(offset, size));
  }

  void AddRange(off_t offset) {
    ranges_.push_back(Range(offset));
  }

  virtual void SetOffset(off_t offset) {}  // for now, doesn't support this

  virtual void SetLength(size_t length) {}  // unsupported
  virtual void UnsetLength() {}

  // Begins the transfer to the specified URL.
  // State change: Stopped -> Downloading
  // (corner case: Stopped -> Stopped for an empty request)
  virtual void BeginTransfer(const std::string& url);

  // State change: Downloading -> Pending transfer ended
  virtual void TerminateTransfer();

  virtual void Pause() { base_fetcher_->Pause(); }

  virtual void Unpause() { base_fetcher_->Unpause(); }

  // These functions are overloaded in LibcurlHttp fetcher for testing purposes.
  virtual void set_idle_seconds(int seconds) {
    base_fetcher_->set_idle_seconds(seconds);
  }
  virtual void set_retry_seconds(int seconds) {
    base_fetcher_->set_retry_seconds(seconds);
  }
  virtual void SetBuildType(bool is_official) {
    base_fetcher_->SetBuildType(is_official);
  }
  inline virtual size_t GetBytesDownloaded() {
    return base_fetcher_->GetBytesDownloaded();
  }

 private:
  // A range object defining the offset and length of a download chunk.  Zero
  // length indicates an unspecified end offset (note that it is impossible to
  // request a zero-length range in HTTP).
  class Range {
   public:
    Range(off_t offset, size_t length) : offset_(offset), length_(length) {}
    Range(off_t offset) : offset_(offset), length_(0) {}

    inline off_t offset() const { return offset_; }
    inline size_t length() const { return length_; }

    inline bool HasLength() const { return (length_ > 0); }

    std::string ToString() const;

   private:
    off_t offset_;
    size_t length_;
  };

  typedef std::vector<Range> RangesVect;

  // State change: Stopped or Downloading -> Downloading
  void StartTransfer();

  // State change: Downloading -> Downloading or Pending transfer ended
  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* bytes,
                             int length);

  // State change: Pending transfer ended -> Stopped
  void TransferEnded(HttpFetcher* fetcher, bool successful);
  // These two call TransferEnded():
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful);
  virtual void TransferTerminated(HttpFetcher* fetcher);

  void Reset();

  scoped_ptr<HttpFetcher> base_fetcher_;

  // If true, do not send any more data or TransferComplete to the delegate.
  bool base_fetcher_active_;

  // If true, the next fetcher needs to be started when TransferTerminated is
  // received from the current fetcher.
  bool pending_transfer_ended_;

  // True if we are waiting for base fetcher to terminate b/c we are
  // ourselves terminating.
  bool terminating_;

  RangesVect ranges_;

  RangesVect::size_type current_index_;  // index into ranges_
  size_t bytes_received_this_range_;

  DISALLOW_COPY_AND_ASSIGN(MultiRangeHttpFetcher);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_MULTI_RANGE_HTTP_FETCHER_H__
