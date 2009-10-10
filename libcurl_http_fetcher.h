// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_LIBCURL_HTTP_FETCHER_H__
#define UPDATE_ENGINE_LIBCURL_HTTP_FETCHER_H__

#include <map>
#include <string>
#include <curl/curl.h>
#include <glib.h>
#include "base/basictypes.h"
#include "glog/logging.h"
#include "update_engine/http_fetcher.h"

// This is a concrete implementation of HttpFetcher that uses libcurl to do the
// http work.

namespace chromeos_update_engine {

class LibcurlHttpFetcher : public HttpFetcher {
 public:
  LibcurlHttpFetcher()
      : curl_multi_handle_(NULL), curl_handle_(NULL),
        timeout_source_(NULL), transfer_in_progress_(false),
        idle_ms_(1000) {}

  // Cleans up all internal state. Does not notify delegate
  ~LibcurlHttpFetcher();

  // Begins the transfer if it hasn't already begun.
  virtual void BeginTransfer(const std::string& url);

  // If the transfer is in progress, aborts the transfer early.
  // The transfer cannot be resumed.
  virtual void TerminateTransfer();

  // Suspend the transfer by calling curl_easy_pause(CURLPAUSE_ALL).
  virtual void Pause();

  // Resume the transfer by calling curl_easy_pause(CURLPAUSE_CONT).
  virtual void Unpause();

  // Libcurl sometimes asks to be called back after some time while
  // leaving that time unspecified. In that case, we pick a reasonable
  // default of one second, but it can be overridden here. This is
  // primarily useful for testing.
  // From http://curl.haxx.se/libcurl/c/curl_multi_timeout.html:
  //     if libcurl returns a -1 timeout here, it just means that libcurl
  //     currently has no stored timeout value. You must not wait too long
  //     (more than a few seconds perhaps) before you call
  //     curl_multi_perform() again.
  void set_idle_ms(long ms) {
    idle_ms_ = ms;
  }
 private:

  // These two methods are for glib main loop callbacks. They are called
  // when either a file descriptor is ready for work or when a timer
  // has fired. The static versions are shims for libcurl which has a C API.
  bool FDCallback(GIOChannel *source, GIOCondition condition);
  static gboolean StaticFDCallback(GIOChannel *source,
                                   GIOCondition condition,
                                   gpointer data) {
    return reinterpret_cast<LibcurlHttpFetcher*>(data)->FDCallback(source,
                                                                   condition);
  }
  bool TimeoutCallback();
  static gboolean StaticTimeoutCallback(gpointer data) {
    return reinterpret_cast<LibcurlHttpFetcher*>(data)->TimeoutCallback();
  }

  // Calls into curl_multi_perform to let libcurl do its work. Returns after
  // curl_multi_perform is finished, which may actually be after more than
  // one call to curl_multi_perform. This method will set up the glib run
  // loop with sources for future work that libcurl will do.
  // This method will not block.
  void CurlPerformOnce();

  // Sets up glib main loop sources as needed by libcurl. This is generally
  // the file descriptor of the socket and a timer in case nothing happens
  // on the fds.
  void SetupMainloopSources();

  // Callback called by libcurl when new data has arrived on the transfer
  size_t LibcurlWrite(void *ptr, size_t size, size_t nmemb);
  static size_t StaticLibcurlWrite(void *ptr, size_t size,
                                   size_t nmemb, void *stream) {
    return reinterpret_cast<LibcurlHttpFetcher*>(stream)->
        LibcurlWrite(ptr, size, nmemb);
  }

  // Cleans up the following if they are non-null:
  // curl(m) handles, io_channels_, timeout_source_.
  void CleanUp();

  // Handles for the libcurl library
  CURLM *curl_multi_handle_;
  CURL *curl_handle_;

  // a list of all file descriptors that we're waiting on from the
  // glib main loop
  typedef std::map<int, std::pair<GIOChannel*, guint> > IOChannels;
  IOChannels io_channels_;

  // if non-NULL, a timer we're waiting on. glib main loop will call us back
  // when it fires.
  GSource* timeout_source_;

  bool transfer_in_progress_;

  long idle_ms_;
  DISALLOW_COPY_AND_ASSIGN(LibcurlHttpFetcher);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_LIBCURL_HTTP_FETCHER_H__
