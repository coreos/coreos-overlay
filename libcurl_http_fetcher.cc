// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/libcurl_http_fetcher.h"
#include <algorithm>
#include "base/logging.h"

using std::max;
using std::make_pair;

// This is a concrete implementation of HttpFetcher that uses libcurl to do the
// http work.

namespace chromeos_update_engine {

namespace {
const int kMaxRetriesCount = 20;
}

LibcurlHttpFetcher::~LibcurlHttpFetcher() {
  CleanUp();
}

void LibcurlHttpFetcher::ResumeTransfer(const std::string& url) {
  LOG(INFO) << "Starting/Resuming transfer";
  CHECK(!transfer_in_progress_);
  url_ = url;
  curl_multi_handle_ = curl_multi_init();
  CHECK(curl_multi_handle_);

  curl_handle_ = curl_easy_init();
  CHECK(curl_handle_);

  if (post_data_set_) {
    CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_POST, 1), CURLE_OK);
    CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS,
                              &post_data_[0]),
             CURLE_OK);
    CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDSIZE,
                              post_data_.size()),
             CURLE_OK);
  }

  if (bytes_downloaded_ > 0) {
    // Resume from where we left off
    resume_offset_ = bytes_downloaded_;
    CHECK_EQ(curl_easy_setopt(curl_handle_,
                              CURLOPT_RESUME_FROM_LARGE,
                              bytes_downloaded_), CURLE_OK);
  }

  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, this), CURLE_OK);
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION,
                            StaticLibcurlWrite), CURLE_OK);
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_URL, url_.c_str()), CURLE_OK);

  // If the connection drops under 10 bytes/sec for 3 minutes, reconnect.
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_LOW_SPEED_LIMIT, 10),
           CURLE_OK);
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_LOW_SPEED_TIME, 3 * 60),
           CURLE_OK);

  CHECK_EQ(curl_multi_add_handle(curl_multi_handle_, curl_handle_), CURLM_OK);
  transfer_in_progress_ = true;
}

// Begins the transfer, which must not have already been started.
void LibcurlHttpFetcher::BeginTransfer(const std::string& url) {
  transfer_size_ = -1;
  bytes_downloaded_ = 0;
  resume_offset_ = 0;
  retry_count_ = 0;
  ResumeTransfer(url);
  CurlPerformOnce();
}

void LibcurlHttpFetcher::TerminateTransfer() {
  CleanUp();
}

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
    long http_response_code = 0;
    if (curl_easy_getinfo(curl_handle_,
                          CURLINFO_RESPONSE_CODE,
                          &http_response_code) == CURLE_OK) {
      LOG(INFO) << "HTTP response code: " << http_response_code;
    } else {
      LOG(ERROR) << "Unable to get http response code.";
    }

    // we're done!
    CleanUp();

    if ((transfer_size_ >= 0) && (bytes_downloaded_ < transfer_size_)) {
      // Need to restart transfer
      retry_count_++;
      LOG(INFO) << "Restarting transfer b/c we finished, had downloaded "
                << bytes_downloaded_ << " bytes, but transfer_size_ is "
                << transfer_size_ << ". retry_count: " << retry_count_;
      if (retry_count_ > kMaxRetriesCount) {
        if (delegate_)
          delegate_->TransferComplete(this, false);  // success
      } else {
        g_timeout_add(5 * 1000,
                      &LibcurlHttpFetcher::StaticRetryTimeoutCallback,
                      this);
      }
      return;
    } else {
      if (delegate_) {
        // success is when http_response_code is 2xx
        bool success = (http_response_code >= 200) &&
            (http_response_code < 300);
        delegate_->TransferComplete(this, success);
      }
    }
  } else {
    // set up callback
    SetupMainloopSources();
  }
}

size_t LibcurlHttpFetcher::LibcurlWrite(void *ptr, size_t size, size_t nmemb) {
  {
    double transfer_size_double;
    CHECK_EQ(curl_easy_getinfo(curl_handle_,
                               CURLINFO_CONTENT_LENGTH_DOWNLOAD,
                               &transfer_size_double), CURLE_OK);
    off_t new_transfer_size = static_cast<off_t>(transfer_size_double);
    if (new_transfer_size > 0) {
      transfer_size_ = resume_offset_ + new_transfer_size;
    }
  }
  bytes_downloaded_ += size * nmemb;
  if (delegate_)
    delegate_->ReceivedBytes(this, reinterpret_cast<char*>(ptr), size * nmemb);
  return size * nmemb;
}

void LibcurlHttpFetcher::Pause() {
  CHECK(curl_handle_);
  CHECK(transfer_in_progress_);
  CHECK_EQ(curl_easy_pause(curl_handle_, CURLPAUSE_ALL), CURLE_OK);
}

void LibcurlHttpFetcher::Unpause() {
  CHECK(curl_handle_);
  CHECK(transfer_in_progress_);
  CHECK_EQ(curl_easy_pause(curl_handle_, CURLPAUSE_CONT), CURLE_OK);
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
  CHECK_EQ(curl_multi_fdset(curl_multi_handle_, &fd_read, &fd_write,
                            &fd_exec, &fd_max), CURLM_OK);

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
    static int io_counter = 0;
    io_counter++;
    if (io_counter % 50 == 0) {
      LOG(INFO) << "io_counter = " << io_counter;
    }
  }

  // Wet up a timeout callback for libcurl
  long ms = 0;
  CHECK_EQ(curl_multi_timeout(curl_multi_handle_, &ms), CURLM_OK);
  if (ms < 0) {
    // From http://curl.haxx.se/libcurl/c/curl_multi_timeout.html:
    //     if libcurl returns a -1 timeout here, it just means that libcurl
    //     currently has no stored timeout value. You must not wait too long
    //     (more than a few seconds perhaps) before you call
    //     curl_multi_perform() again.
    ms = idle_ms_;
  }
  if (!timeout_source_) {
    LOG(INFO) << "setting up timeout source:" << ms;
    timeout_source_ = g_timeout_source_new(1000);
    CHECK(timeout_source_);
    g_source_set_callback(timeout_source_, StaticTimeoutCallback, this,
                          NULL);
    g_source_attach(timeout_source_, NULL);
    static int counter = 0;
    counter++;
    if (counter % 50 == 0) {
      LOG(INFO) << "counter = " << counter;
    }
  }
}

bool LibcurlHttpFetcher::FDCallback(GIOChannel *source,
                                    GIOCondition condition) {
  CurlPerformOnce();
  // We handle removing of this source elsewhere, so we always return true.
  // The docs say, "the function should return FALSE if the event source
  // should be removed."
  // http://www.gtk.org/api/2.6/glib/glib-IO-Channels.html#GIOFunc
  return true;
}

gboolean LibcurlHttpFetcher::RetryTimeoutCallback() {
  ResumeTransfer(url_);
  CurlPerformOnce();
  return FALSE;  // Don't have glib auto call this callback again
}

gboolean LibcurlHttpFetcher::TimeoutCallback() {
  // We always return true, even if we don't want glib to call us back.
  // We will remove the event source separately if we don't want to
  // be called back.
  if (!transfer_in_progress_)
    return TRUE;
  CurlPerformOnce();
  return TRUE;
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
      CHECK_EQ(curl_multi_remove_handle(curl_multi_handle_, curl_handle_),
               CURLM_OK);
    }
    curl_easy_cleanup(curl_handle_);
    curl_handle_ = NULL;
  }
  if (curl_multi_handle_) {
    CHECK_EQ(curl_multi_cleanup(curl_multi_handle_), CURLM_OK);
    curl_multi_handle_ = NULL;
  }
  transfer_in_progress_ = false;
}

}  // namespace chromeos_update_engine
