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
  HttpFetcher() : post_data_set_(false), delegate_(NULL) {}
  virtual ~HttpFetcher() {}
  void set_delegate(HttpFetcherDelegate* delegate) {
    delegate_ = delegate;
  }
  HttpFetcherDelegate* delegate() const {
    return delegate_;
  }

  // Optional: Post data to the server. The HttpFetcher should make a copy
  // of this data and upload it via HTTP POST during the transfer.
  void SetPostData(const void* data, size_t size) {
    post_data_set_ = true;
    post_data_.clear();
    const char *char_data = reinterpret_cast<const char*>(data);
    post_data_.insert(post_data_.end(), char_data, char_data + size);
  }

  // Begins the transfer to the specified URL.
  virtual void BeginTransfer(const std::string& url) = 0;

  // Aborts the transfer. TransferComplete() will not be called on the
  // delegate.
  virtual void TerminateTransfer() = 0;

  // If data is coming in too quickly, you can call Pause() to pause the
  // transfer. The delegate will not have ReceivedBytes() called while
  // an HttpFetcher is paused.
  virtual void Pause() = 0;

  // Used to unpause an HttpFetcher and let the bytes stream in again.
  // If a delegate is set, ReceivedBytes() may be called on it before
  // Unpause() returns
  virtual void Unpause() = 0;

 protected:
  // The URL we're actively fetching from
  std::string url_;

  // POST data for the transfer, and whether or not it was ever set
  bool post_data_set_;
  std::vector<char> post_data_;

  // The delegate; may be NULL.
  HttpFetcherDelegate* delegate_;
 private:
  DISALLOW_COPY_AND_ASSIGN(HttpFetcher);
};

// Interface for delegates
class HttpFetcherDelegate {
 public:
  // Called every time bytes are received, even if they are automatically
  // delivered to an output file.
  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* bytes,
                             int length) = 0;

  // Called when the transfer has completed successfully or been somehow
  // aborted.
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) = 0;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_HTTP_FETCHER_H__
