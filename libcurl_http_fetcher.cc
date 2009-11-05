// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "update_engine/libcurl_http_fetcher.h"

// This is a concrete implementation of HttpFetcher that uses libcurl to do the
// http work.

namespace chromeos_update_engine {

LibcurlHttpFetcher::~LibcurlHttpFetcher() {
  CleanUp();
}

// Begins the transfer, which must not have already been started.
void LibcurlHttpFetcher::BeginTransfer(const std::string& url) {
  CHECK(!transfer_in_progress_);
  url_ = url;
  curl_multi_handle_ = curl_multi_init();
  CHECK(curl_multi_handle_);

  curl_handle_ = curl_easy_init();
  CHECK(curl_handle_);

  if (post_data_set_) {
    CHECK_EQ(CURLE_OK, curl_easy_setopt(curl_handle_, CURLOPT_POST, 1));
    CHECK_EQ(CURLE_OK, curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS,
                                        &post_data_[0]));
    CHECK_EQ(CURLE_OK, curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDSIZE,
                                        post_data_.size()));
  }

  CHECK_EQ(CURLE_OK, curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, this));
  CHECK_EQ(CURLE_OK, curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION,
                                      StaticLibcurlWrite));
  CHECK_EQ(CURLE_OK, curl_easy_setopt(curl_handle_, CURLOPT_URL, url_.c_str()));
  CHECK_EQ(CURLM_OK, curl_multi_add_handle(curl_multi_handle_, curl_handle_));
  transfer_in_progress_ = true;
  CurlPerformOnce();
}

void LibcurlHttpFetcher::TerminateTransfer() {
  CleanUp();
}

// TODO(adlr): detect network failures
void LibcurlHttpFetcher::CurlPerformOnce() {
  CHECK(transfer_in_progress_);
  int running_handles = 0;
  CURLMcode retcode = CURLM_CALL_MULTI_PERFORM;

  // libcurl may request that we immediately call curl_multi_perform after it
  // returns, so we do. libcurl promises that curl_multi_perform will not block.
  while (CURLM_CALL_MULTI_PERFORM == retcode) {
    retcode = curl_multi_perform(curl_multi_handle_, &running_handles);
  }
  if (0 == running_handles) {
    // we're done!
    CleanUp();
    if (delegate_)
      delegate_->TransferComplete(this, true);  // success
  } else {
    // set up callback
    SetupMainloopSources();
  }
}

size_t LibcurlHttpFetcher::LibcurlWrite(void *ptr, size_t size, size_t nmemb) {
  if (delegate_)
    delegate_->ReceivedBytes(this, reinterpret_cast<char*>(ptr), size * nmemb);
  return size * nmemb;
}

void LibcurlHttpFetcher::Pause() {
  CHECK(curl_handle_);
  CHECK(transfer_in_progress_);
  CHECK_EQ(CURLE_OK, curl_easy_pause(curl_handle_, CURLPAUSE_ALL));
}

void LibcurlHttpFetcher::Unpause() {
  CHECK(curl_handle_);
  CHECK(transfer_in_progress_);
  CHECK_EQ(CURLE_OK, curl_easy_pause(curl_handle_, CURLPAUSE_CONT));
}

// This method sets up callbacks with the glib main loop.
void LibcurlHttpFetcher::SetupMainloopSources() {
  fd_set fd_read;
  fd_set fd_write;
  fd_set fd_exec;

  FD_ZERO(&fd_read);
  FD_ZERO(&fd_write);
  FD_ZERO(&fd_exec);

  int fd_max = 0;

  // Ask libcurl for the set of file descriptors we should track on its
  // behalf.
  CHECK_EQ(CURLM_OK, curl_multi_fdset(curl_multi_handle_, &fd_read, &fd_write,
                                      &fd_exec, &fd_max));

  // We should iterate through all file descriptors up to libcurl's fd_max or
  // the highest one we're tracking, whichever is larger
  if (!io_channels_.empty())
    fd_max = max(fd_max, io_channels_.rbegin()->first);

  // For each fd, if we're not tracking it, track it. If we are tracking it,
  // but libcurl doesn't care about it anymore, stop tracking it.
  // After this loop, there should be exactly as many GIOChannel objects
  // in io_channels_ as there are fds that we're tracking.
  for (int i = 0; i <= fd_max; i++) {
    if (!(FD_ISSET(i, &fd_read) || FD_ISSET(i, &fd_write) ||
        FD_ISSET(i, &fd_exec))) {
      // if we have an outstanding io_channel, remove it
      if (io_channels_.find(i) != io_channels_.end()) {
        g_source_remove(io_channels_[i].second);
        g_io_channel_unref(io_channels_[i].first);
        io_channels_.erase(io_channels_.find(i));
      }
      continue;
    }
    // If we are already tracking this fd, continue.
    if (io_channels_.find(i) != io_channels_.end())
      continue;

    // We must track a new fd
    GIOChannel *io_channel = g_io_channel_unix_new(i);
    guint tag = g_io_add_watch(
        io_channel,
        static_cast<GIOCondition>(G_IO_IN | G_IO_OUT | G_IO_PRI |
                                  G_IO_ERR | G_IO_HUP),
        &StaticFDCallback,
        this);
    io_channels_[i] = make_pair(io_channel, tag);
  }

  // Wet up a timeout callback for libcurl
  long ms = 0;
  CHECK_EQ(CURLM_OK, curl_multi_timeout(curl_multi_handle_, &ms));
  if (ms < 0) {
    // From http://curl.haxx.se/libcurl/c/curl_multi_timeout.html:
    //     if libcurl returns a -1 timeout here, it just means that libcurl
    //     currently has no stored timeout value. You must not wait too long
    //     (more than a few seconds perhaps) before you call
    //     curl_multi_perform() again.
    ms = idle_ms_;
  }
  if (timeout_source_) {
    g_source_destroy(timeout_source_);
    timeout_source_ = NULL;
  }
  timeout_source_ = g_timeout_source_new(ms);
  CHECK(timeout_source_);
  g_source_set_callback(timeout_source_, StaticTimeoutCallback, this,
                        NULL);
  g_source_attach(timeout_source_, NULL);
}

bool LibcurlHttpFetcher::FDCallback(GIOChannel *source,
                                    GIOCondition condition) {
  // Figure out which source it was; hopefully there aren't too many b/c
  // this is a linear scan of our channels
  bool found_in_set = false;
  for (IOChannels::iterator it = io_channels_.begin();
       it != io_channels_.end(); ++it) {
    if (it->second.first == source) {
      // We will return false from this method, meaning that we shouldn't keep
      // this g_io_channel around. So we remove it now from our collection of
      // g_io_channels so that the other code in this class doens't mess with
      // this (doomed) GIOChannel.
      // TODO(adlr): optimize by seeing if we should reuse this GIOChannel
      g_source_remove(it->second.second);
      g_io_channel_unref(it->second.first);
      io_channels_.erase(it);
      found_in_set = true;
      break;
    }
  }
  CHECK(found_in_set);
  CurlPerformOnce();
  return false;
}

bool LibcurlHttpFetcher::TimeoutCallback() {
  // Since we will return false from this function, which tells glib to
  // destroy the timeout callback, we must NULL it out here. This way, when
  // setting up callback sources again, we won't try to delete this (doomed)
  // timeout callback then.
  // TODO(adlr): optimize by checking if we can keep this timeout callback.
  timeout_source_ = NULL;
  CurlPerformOnce();
  return false;
}

void LibcurlHttpFetcher::CleanUp() {
  if (timeout_source_) {
    g_source_destroy(timeout_source_);
    timeout_source_ = NULL;
  }

  for (IOChannels::iterator it = io_channels_.begin();
       it != io_channels_.end(); ++it) {
    g_source_remove(it->second.second);
    g_io_channel_unref(it->second.first);
  }
  io_channels_.clear();

  if (curl_handle_) {
    if (curl_multi_handle_) {
      CHECK_EQ(CURLM_OK,
               curl_multi_remove_handle(curl_multi_handle_, curl_handle_));
    }
    curl_easy_cleanup(curl_handle_);
    curl_handle_ = NULL;
  }
  if (curl_multi_handle_) {
    CHECK_EQ(CURLM_OK, curl_multi_cleanup(curl_multi_handle_));
    curl_multi_handle_ = NULL;
  }
  transfer_in_progress_ = false;
}

}  // namespace chromeos_update_engine
