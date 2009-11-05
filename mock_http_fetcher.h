// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_MOCK_HTTP_FETCHER_H__
#define UPDATE_ENGINE_MOCK_HTTP_FETCHER_H__

#include <vector>
#include <glib.h>
#include <base/logging.h>
#include "update_engine/http_fetcher.h"

// This is a mock implementation of HttpFetcher which is useful for testing.
// All data must be passed into the ctor. When started, MockHttpFetcher will
// deliver the data in chunks of size kMockHttpFetcherChunkSize. To simulate
// a network failure, you can call FailTransfer().

namespace chromeos_update_engine {

// MockHttpFetcher will send a chunk of data down in each call to BeginTransfer
// and Unpause. For the other chunks of data, a callback is put on the run
// loop and when that's called, another chunk is sent down.
const size_t kMockHttpFetcherChunkSize(65536);

class MockHttpFetcher : public HttpFetcher {
 public:
  // The data passed in here is copied and then passed to the delegate after
  // the transfer begins.
  MockHttpFetcher(const char* data, size_t size)
      : sent_size_(0), timeout_source_(NULL), timout_tag_(0), paused_(false) {
    data_.insert(data_.end(), data, data + size);
    LOG(INFO) << "timeout_source_ = " << (int)timeout_source_;
  }

  // Cleans up all internal state. Does not notify delegate
  ~MockHttpFetcher();

  // Begins the transfer if it hasn't already begun.
  virtual void BeginTransfer(const std::string& url);

  // If the transfer is in progress, aborts the transfer early.
  // The transfer cannot be resumed.
  virtual void TerminateTransfer();

  // Suspend the mock transfer.
  virtual void Pause();

  // Resume the mock transfer.
  virtual void Unpause();

  // Fail the transfer. This simulates a network failure.
  void FailTransfer();

  const std::vector<char>& post_data() const {
    return post_data_;
  }
  
 private:
  // Sends data to the delegate and sets up a glib timeout callback if needed.
  // There must be a delegate and there must be data to send. If there is
  // already a timeout callback, and it should be deleted by the caller,
  // this will return false; otherwise true is returned.
  // If skip_delivery is true, no bytes will be delivered, but the callbacks
  // still still be set if needed
  bool SendData(bool skip_delivery);

  // Callback for when our glib main loop callback is called
  bool TimeoutCallback();
  static gboolean StaticTimeoutCallback(gpointer data) {
    return reinterpret_cast<MockHttpFetcher*>(data)->TimeoutCallback();
  }

  // A full copy of the data we'll return to the delegate
  std::vector<char> data_;

  // The number of bytes we've sent so far
  size_t sent_size_;

  // The glib main loop timeout source. After each chunk of data sent, we
  // time out for 0s just to make sure that run loop services other clients.
  GSource* timeout_source_;

  // ID of the timeout source, valid only if timeout_source_ != NULL
  guint timout_tag_;

  // True iff the fetcher is paused.
  bool paused_;

  DISALLOW_COPY_AND_ASSIGN(MockHttpFetcher);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_MOCK_HTTP_FETCHER_H__
