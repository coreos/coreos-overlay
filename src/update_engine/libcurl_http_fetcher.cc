// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/libcurl_http_fetcher.h"

#include <algorithm>
#include <string>

#include <glog/logging.h>

#include "strings/string_printf.h"
#include "update_engine/certificate_checker.h"
#include "update_engine/dbus_interface.h"
#include "update_engine/utils.h"

using google::protobuf::NewCallback;
using std::max;
using std::make_pair;
using std::string;
using strings::StringPrintf;

// This is a concrete implementation of HttpFetcher that uses libcurl to do the
// http work.

namespace chromeos_update_engine {

namespace {
const int kNoNetworkRetrySeconds = 10;
const char kCACertificatesPath[] = "/etc/ssl/certs";
}  // namespace {}

const int LibcurlHttpFetcher::kMaxRedirects = 10;
const int LibcurlHttpFetcher::kMaxRetryCountOobeComplete = 20;
const int LibcurlHttpFetcher::kMaxRetryCountOobeNotComplete = 3;

LibcurlHttpFetcher::~LibcurlHttpFetcher() {
  LOG_IF(ERROR, transfer_in_progress_)
      << "Destroying the fetcher while a transfer is in progress.";
  CleanUp();
}

bool LibcurlHttpFetcher::IsOfficialBuild() const {
  return force_build_type_ ? forced_official_build_ : utils::IsOfficialBuild();
}

void LibcurlHttpFetcher::ResumeTransfer(const std::string& url) {
  LOG(INFO) << "Starting/Resuming transfer";
  CHECK(!transfer_in_progress_);
  url_ = url;
  curl_multi_handle_ = curl_multi_init();
  CHECK(curl_multi_handle_);

  curl_handle_ = curl_easy_init();
  CHECK(curl_handle_);

  CHECK_EQ(curl_easy_setopt(curl_handle_,
                            CURLOPT_ERRORBUFFER,
                            &curl_error_buffer_), CURLE_OK);

  if (post_data_set_) {
    CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_POST, 1), CURLE_OK);
    CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS,
                              &post_data_[0]),
             CURLE_OK);
    CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDSIZE,
                              post_data_.size()),
             CURLE_OK);

    // Set the Content-Type HTTP header, if one was specifically set.
    CHECK(!curl_http_headers_);
    if (post_content_type_ != kHttpContentTypeUnspecified) {
      const string content_type_attr =
          StringPrintf("Content-Type: %s",
                       GetHttpContentTypeString(post_content_type_));
      curl_http_headers_ = curl_slist_append(NULL, content_type_attr.c_str());
      CHECK(curl_http_headers_);
      CHECK_EQ(
          curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER,
                           curl_http_headers_),
          CURLE_OK);
    } else {
      LOG(WARNING) << "no content type set, using libcurl default";
    }
  }

  if (bytes_downloaded_ > 0 || download_length_) {
    // Resume from where we left off.
    resume_offset_ = bytes_downloaded_;
    CHECK_GE(resume_offset_, 0);

    // Compute end offset, if one is specified. As per HTTP specification, this
    // is an inclusive boundary. Make sure it doesn't overflow.
    size_t end_offset = 0;
    if (download_length_) {
      end_offset = static_cast<size_t>(resume_offset_) + download_length_ - 1;
      CHECK_LE((size_t) resume_offset_, end_offset);
    }

    // Create a string representation of the desired range.
    std::string range_str = (end_offset ?
                             StringPrintf("%jd-%zu", resume_offset_,
                                          end_offset) :
                             StringPrintf("%jd-", resume_offset_));
    CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_RANGE, range_str.c_str()),
             CURLE_OK);
  }

  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, this), CURLE_OK);
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION,
                            StaticLibcurlWrite), CURLE_OK);

  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_URL, url_.c_str()),
           CURLE_OK);

  // If the connection drops under 10 bytes/sec for 3 minutes, reconnect.
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_LOW_SPEED_LIMIT, 10),
           CURLE_OK);
  // Use a smaller timeout on official builds, larger for dev. Dev users
  // want a longer timeout b/c they may be waiting on the dev server to
  // build an image.
  const int kTimeout = IsOfficialBuild() ? 90 : 3 * 60;
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_LOW_SPEED_TIME, kTimeout),
           CURLE_OK);
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_CONNECTTIMEOUT, 30),
           CURLE_OK);

  // By default, libcurl doesn't follow redirections. Allow up to
  // |kMaxRedirects| redirections.
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_FOLLOWLOCATION, 1), CURLE_OK);
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_MAXREDIRS, kMaxRedirects),
           CURLE_OK);

  // For the sake of security if this is an official build lock down
  // the appropriate curl options for HTTP or HTTPS depending on the url.
  if (IsOfficialBuild()) {
    if (utils::IsHTTPS(url_))
      SetCurlOptionsForHttps();
    else
      SetCurlOptionsForHttp();
  } else {
    LOG(INFO) << "Not setting http(s) curl options because we are in "
              << "test mode or running a dev/test image";
  }

  CHECK_EQ(curl_multi_add_handle(curl_multi_handle_, curl_handle_), CURLM_OK);
  transfer_in_progress_ = true;
}

// Lock down only the protocol in case of HTTP.
void LibcurlHttpFetcher::SetCurlOptionsForHttp() {
  LOG(INFO) << "Setting up curl options for HTTP";
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_PROTOCOLS, CURLPROTO_HTTP),
           CURLE_OK);
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_REDIR_PROTOCOLS,
                            CURLPROTO_HTTP),
           CURLE_OK);
}

// Security lock-down in official builds: makes sure that peer certificate
// verification is enabled, restricts the set of trusted certificates,
// restricts protocols to HTTPS, restricts ciphers to HIGH, disables SSLv3.
void LibcurlHttpFetcher::SetCurlOptionsForHttps() {
  LOG(INFO) << "Setting up curl options for HTTPS";
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_SSL_VERIFYPEER, 1),
           CURLE_OK);
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_CAPATH, kCACertificatesPath),
           CURLE_OK);
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS),
           CURLE_OK);
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_REDIR_PROTOCOLS,
                            CURLPROTO_HTTPS),
           CURLE_OK);
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_SSL_CIPHER_LIST, "HIGH:!ADH"),
           CURLE_OK);
  CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_SSLVERSION,
                            CURL_SSLVERSION_TLSv1),
           CURLE_OK);
  if (check_certificate_ != CertificateChecker::kNone) {
    CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_SSL_CTX_DATA,
                              &check_certificate_),
             CURLE_OK);
    CHECK_EQ(curl_easy_setopt(curl_handle_, CURLOPT_SSL_CTX_FUNCTION,
                              CertificateChecker::ProcessSSLContext),
             CURLE_OK);
  }
}


// Begins the transfer, which must not have already been started.
void LibcurlHttpFetcher::BeginTransfer(const std::string& url) {
  CHECK(!transfer_in_progress_);
  url_ = url;
  transfer_size_ = -1;
  resume_offset_ = 0;
  retry_count_ = 0;
  max_retry_count_ = kMaxRetryCountOobeComplete;

  no_network_retry_count_ = 0;
  http_response_code_ = 0;
  terminate_requested_ = false;
  sent_byte_ = false;
  ResumeTransfer(url_);
  CurlPerformOnce();
}

void LibcurlHttpFetcher::ForceTransferTermination() {
  CleanUp();
  if (delegate_) {
    // Note that after the callback returns this object may be destroyed.
    delegate_->TransferTerminated(this);
  }
}

void LibcurlHttpFetcher::TerminateTransfer() {
  if (in_write_callback_) {
    terminate_requested_ = true;
  } else {
    ForceTransferTermination();
  }
}

void LibcurlHttpFetcher::CurlPerformOnce() {
  CHECK(transfer_in_progress_);
  int running_handles = 0;
  CURLMcode retcode = CURLM_CALL_MULTI_PERFORM;

  // libcurl may request that we immediately call curl_multi_perform after it
  // returns, so we do. libcurl promises that curl_multi_perform will not block.
  while (CURLM_CALL_MULTI_PERFORM == retcode) {
    retcode = curl_multi_perform(curl_multi_handle_, &running_handles);
    if (terminate_requested_) {
      ForceTransferTermination();
      return;
    }
  }
  if (0 == running_handles) {
    GetHttpResponseCode();
    if (http_response_code_) {
      LOG(INFO) << "HTTP response code: " << http_response_code_;
      no_network_retry_count_ = 0;
    } else {
      LOG(ERROR) << "Unable to get http response code: " << curl_error_buffer_;
    }

    // we're done!
    CleanUp();

    // TODO(petkov): This temporary code tries to deal with the case where the
    // update engine performs an update check while the network is not ready
    // (e.g., right after resume). Longer term, we should check if the network
    // is online/offline and return an appropriate error code.
    if (!sent_byte_ &&
        http_response_code_ == 0 &&
        no_network_retry_count_ < no_network_max_retries_) {
      no_network_retry_count_++;
      g_timeout_add_seconds(kNoNetworkRetrySeconds,
                            &LibcurlHttpFetcher::StaticRetryTimeoutCallback,
                            this);
      LOG(INFO) << "No HTTP response, retry " << no_network_retry_count_;
      return;
    }

    if ((!sent_byte_ && !IsHttpResponseSuccess()) || IsHttpResponseError()) {
      // The transfer completed w/ error and we didn't get any bytes.
      //
      // TODO(garnold) in fact there are two separate cases here: one case is an
      // other-than-success return code (including no return code) and no
      // received bytes, which is necessary due to the way callbacks are
      // currently processing error conditions;  the second is an explicit HTTP
      // error code, where some data may have been received (as in the case of a
      // semi-successful multi-chunk fetch).  This is a confusing behavior and
      // should be unified into a complete, coherent interface.
      LOG(INFO) << "Transfer resulted in an error (" << http_response_code_
                << "), " << bytes_downloaded_ << " bytes downloaded";
      if (delegate_)
        delegate_->TransferComplete(this, false);  // signal fail
    } else if ((transfer_size_ >= 0) && (bytes_downloaded_ < transfer_size_)) {
      retry_count_++;
      LOG(INFO) << "Transfer interrupted after downloading "
                << bytes_downloaded_ << " of " << transfer_size_ << " bytes. "
                << transfer_size_ - bytes_downloaded_ << " bytes remaining "
                << "after " << retry_count_ << " attempt(s)";

      if (retry_count_ > max_retry_count_) {
        LOG(INFO) << "Reached max attempts (" << retry_count_ << ")";
        if (delegate_)
          delegate_->TransferComplete(this, false);  // signal fail
      } else {
        // Need to restart transfer
        LOG(INFO) << "Restarting transfer to download the remaining bytes";
        g_timeout_add_seconds(retry_seconds_,
                              &LibcurlHttpFetcher::StaticRetryTimeoutCallback,
                              this);
      }
    } else {
      LOG(INFO) << "Transfer completed (" << http_response_code_
                << "), " << bytes_downloaded_ << " bytes downloaded";
      if (delegate_) {
        bool success = IsHttpResponseSuccess();
        delegate_->TransferComplete(this, success);
      }
    }
  } else {
    // set up callback
    SetupMainloopSources();
  }
}

size_t LibcurlHttpFetcher::LibcurlWrite(void *ptr, size_t size, size_t nmemb) {
  // Update HTTP response first.
  GetHttpResponseCode();
  const size_t payload_size = size * nmemb;

  // Do nothing if no payload or HTTP response is an error.
  if (payload_size == 0 || !IsHttpResponseSuccess()) {
    LOG(INFO) << "HTTP response unsuccessful (" << http_response_code_
              << ") or no payload (" << payload_size << "), nothing to do";
    return 0;
  }

  sent_byte_ = true;
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
  bytes_downloaded_ += payload_size;
  in_write_callback_ = true;
  if (delegate_)
    delegate_->ReceivedBytes(this, reinterpret_cast<char*>(ptr), payload_size);
  in_write_callback_ = false;
  return payload_size;
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
  fd_set fd_exc;

  FD_ZERO(&fd_read);
  FD_ZERO(&fd_write);
  FD_ZERO(&fd_exc);

  int fd_max = 0;

  // Ask libcurl for the set of file descriptors we should track on its
  // behalf.
  CHECK_EQ(curl_multi_fdset(curl_multi_handle_, &fd_read, &fd_write,
                            &fd_exc, &fd_max), CURLM_OK);

  // We should iterate through all file descriptors up to libcurl's fd_max or
  // the highest one we're tracking, whichever is larger.
  for (size_t t = 0; t < arraysize(io_channels_); ++t) {
    if (!io_channels_[t].empty())
      fd_max = max(fd_max, io_channels_[t].rbegin()->first);
  }

  // For each fd, if we're not tracking it, track it. If we are tracking it, but
  // libcurl doesn't care about it anymore, stop tracking it. After this loop,
  // there should be exactly as many GIOChannel objects in io_channels_[0|1] as
  // there are read/write fds that we're tracking.
  for (int fd = 0; fd <= fd_max; ++fd) {
    // Note that fd_exc is unused in the current version of libcurl so is_exc
    // should always be false.
    bool is_exc = FD_ISSET(fd, &fd_exc) != 0;
    bool must_track[2] = {
      is_exc || (FD_ISSET(fd, &fd_read) != 0),  // track 0 -- read
      is_exc || (FD_ISSET(fd, &fd_write) != 0)  // track 1 -- write
    };

    for (size_t t = 0; t < arraysize(io_channels_); ++t) {
      bool tracked = io_channels_[t].find(fd) != io_channels_[t].end();

      if (!must_track[t]) {
        // If we have an outstanding io_channel, remove it.
        if (tracked) {
          g_source_remove(io_channels_[t][fd].second);
          g_io_channel_unref(io_channels_[t][fd].first);
          io_channels_[t].erase(io_channels_[t].find(fd));
        }
        continue;
      }

      // If we are already tracking this fd, continue -- nothing to do.
      if (tracked)
        continue;

      // Set conditions appropriately -- read for track 0, write for track 1.
      GIOCondition condition = static_cast<GIOCondition>(
          ((t == 0) ? (G_IO_IN | G_IO_PRI) : G_IO_OUT) | G_IO_ERR | G_IO_HUP);

      // Track a new fd.
      GIOChannel* io_channel = g_io_channel_unix_new(fd);
      guint tag =
          g_io_add_watch(io_channel, condition, &StaticFDCallback, this);

      io_channels_[t][fd] = make_pair(io_channel, tag);
      static int io_counter = 0;
      io_counter++;
      if (io_counter % 50 == 0) {
        LOG(INFO) << "io_counter = " << io_counter;
      }
    }
  }

  // Set up a timeout callback for libcurl.
  if (!timeout_source_) {
    LOG(INFO) << "Setting up timeout source: " << idle_seconds_ << " seconds.";
    timeout_source_ = g_timeout_source_new_seconds(idle_seconds_);
    g_source_set_callback(timeout_source_, StaticTimeoutCallback, this, NULL);
    g_source_attach(timeout_source_, NULL);
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

  for (size_t t = 0; t < arraysize(io_channels_); ++t) {
    for (IOChannels::iterator it = io_channels_[t].begin();
         it != io_channels_[t].end(); ++it) {
      g_source_remove(it->second.second);
      g_io_channel_unref(it->second.first);
    }
    io_channels_[t].clear();
  }

  if (curl_http_headers_) {
    curl_slist_free_all(curl_http_headers_);
    curl_http_headers_ = NULL;
  }
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

void LibcurlHttpFetcher::GetHttpResponseCode() {
  long http_response_code = 0;
  if (curl_easy_getinfo(curl_handle_,
                        CURLINFO_RESPONSE_CODE,
                        &http_response_code) == CURLE_OK) {
    http_response_code_ = static_cast<int>(http_response_code);
  }
}

}  // namespace chromeos_update_engine
