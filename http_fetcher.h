// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_HTTP_FETCHER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_HTTP_FETCHER_H__

#include <string>
#include <vector>
#include <glib.h>
#include "base/basictypes.h"

// This class is a simple wrapper around an HTTP library (libcurl). We can
// easily mock out this interface for testing.

// Implementations of this class should use asynchronous i/o. They can access
// the glib main loop to request callbacks when timers or file descriptors
// change.

namespace chromeos_update_engine {

class HttpFetcherDelegate;

class HttpFetcher {
 public:
  HttpFetcher()
      : post_data_set_(false),
        http_response_code_(0),
        delegate_(NULL) {}
  virtual ~HttpFetcher() {}

  void set_delegate(HttpFetcherDelegate* delegate) { delegate_ = delegate; }
  HttpFetcherDelegate* delegate() const { return delegate_; }
  int http_response_code() const { return http_response_code_; }

  // Optional: Post data to the server. The HttpFetcher should make a copy
  // of this data and upload it via HTTP POST during the transfer.
  void SetPostData(const void* data, size_t size) {
    post_data_set_ = true;
    post_data_.clear();
    const char *char_data = reinterpret_cast<const char*>(data);
    post_data_.insert(post_data_.end(), char_data, char_data + size);
  }

  // Downloading should resume from this offset
  virtual void SetOffset(off_t offset) = 0;

  // Begins the transfer to the specified URL. This fetcher instance should not
  // be destroyed until either TransferComplete, or TransferTerminated is
  // called.
  virtual void BeginTransfer(const std::string& url) = 0;

  // Aborts the transfer. The transfer may not abort right away -- delegate's
  // TransferTerminated() will be called when the transfer is actually done.
  virtual void TerminateTransfer() = 0;

  // If data is coming in too quickly, you can call Pause() to pause the
  // transfer. The delegate will not have ReceivedBytes() called while
  // an HttpFetcher is paused.
  virtual void Pause() = 0;

  // Used to unpause an HttpFetcher and let the bytes stream in again.
  // If a delegate is set, ReceivedBytes() may be called on it before
  // Unpause() returns
  virtual void Unpause() = 0;

  // These two function are overloaded in LibcurlHttp fetcher to speed
  // testing.
  virtual void set_idle_seconds(int seconds) {}
  virtual void set_retry_seconds(int seconds) {}

 protected:
  // The URL we're actively fetching from
  std::string url_;

  // POST data for the transfer, and whether or not it was ever set
  bool post_data_set_;
  std::vector<char> post_data_;

  // The server's HTTP response code from the last transfer. This
  // field should be set to 0 when a new transfer is initiated, and
  // set to the response code when the transfer is complete.
  int http_response_code_;

  // The delegate; may be NULL.
  HttpFetcherDelegate* delegate_;
 private:
  DISALLOW_COPY_AND_ASSIGN(HttpFetcher);
};

// Interface for delegates
class HttpFetcherDelegate {
 public:
  // Called every time bytes are received.
  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* bytes,
                             int length) = 0;

  // Called if the fetcher seeks to a particular offset.
  virtual void SeekToOffset(off_t offset) {}

  // Called when the transfer has completed successfully or been aborted through
  // means other than TerminateTransfer. It's OK to destroy the |fetcher| object
  // in this callback.
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) = 0;

  // Called when the transfer has been aborted through TerminateTransfer. It's
  // OK to destroy the |fetcher| object in this callback.
  virtual void TransferTerminated(HttpFetcher* fetcher) {}
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_HTTP_FETCHER_H__
