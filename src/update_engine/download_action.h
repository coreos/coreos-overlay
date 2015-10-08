// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DOWNLOAD_ACTION_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DOWNLOAD_ACTION_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <memory>
#include <string>

#include <curl/curl.h>
#include <google/protobuf/stubs/common.h>

#include "update_engine/action.h"
#include "update_engine/http_fetcher.h"
#include "update_engine/install_plan.h"
#include "update_engine/payload_processor.h"

// The Download Action downloads a specified url to disk. The url should point
// to an update in a delta payload format. The payload will be piped into a
// DeltaPerformer that will apply the delta to the disk.

namespace chromeos_update_engine {

class DownloadActionDelegate {
 public:
  // Called right before starting the download with |active| set to
  // true. Called after completing the download with |active| set to
  // false.
  virtual void SetDownloadStatus(bool active) = 0;

  // Called periodically after bytes are received. This method will be
  // invoked only if the download is active. |received| is the number of
  // bytes just received, |progress| is the total number of bytes
  // downloaded thus far. |total| is the number of bytes expected.
  virtual void BytesReceived(uint64_t received,
                             uint64_t progress,
                             uint64_t total) = 0;
};

class DownloadAction;
class NoneType;
class PrefsInterface;

template<>
class ActionTraits<DownloadAction> {
 public:
  // Takes and returns an InstallPlan
  typedef InstallPlan InputObjectType;
  typedef InstallPlan OutputObjectType;
};

class DownloadAction : public Action<DownloadAction>,
                       public HttpFetcherDelegate {
 public:
  // Takes ownership of the passed in HttpFetcher. Useful for testing.
  // A good calling pattern is:
  // DownloadAction(prefs, new WhateverHttpFetcher);
  DownloadAction(PrefsInterface* prefs, HttpFetcher* http_fetcher);
  virtual ~DownloadAction();
  typedef ActionTraits<DownloadAction>::InputObjectType InputObjectType;
  typedef ActionTraits<DownloadAction>::OutputObjectType OutputObjectType;
  void PerformAction();
  void TerminateProcessing();

  // Testing
  void SetTestFileWriter(FileWriter* writer) {
    writer_ = writer;
  }

  int GetHTTPResponseCode() { return http_fetcher_->http_response_code(); }

  // Debugging/logging
  static std::string StaticType() { return "DownloadAction"; }
  std::string Type() const { return StaticType(); }

  // HttpFetcherDelegate methods (see http_fetcher.h)
  virtual void ReceivedBytes(HttpFetcher *fetcher,
                             const char* bytes, int length);
  virtual void SeekToOffset(off_t offset);
  virtual void TransferComplete(HttpFetcher *fetcher, bool successful);
  virtual void TransferTerminated(HttpFetcher *fetcher);

  DownloadActionDelegate* delegate() const { return delegate_; }
  void set_delegate(DownloadActionDelegate* delegate) {
    delegate_ = delegate;
  }

  HttpFetcher* http_fetcher() { return http_fetcher_.get(); }

 private:
  // The InstallPlan passed in
  InstallPlan install_plan_;

  // Update Engine preference store.
  PrefsInterface* prefs_;

  // Pointer to the HttpFetcher that does the http work.
  std::unique_ptr<HttpFetcher> http_fetcher_;

  // The FileWriter that downloaded data should be written to. It will
  // either point to *decompressing_file_writer_ or *manifest_processor_;
  FileWriter* writer_;

  std::unique_ptr<PayloadProcessor> payload_processor_;

  // Used by TransferTerminated to figure if this action terminated itself or
  // was terminated by the action processor.
  ActionExitCode code_;

  // For reporting status to outsiders
  DownloadActionDelegate* delegate_;
  uint64_t bytes_downloaded_;

  DISALLOW_COPY_AND_ASSIGN(DownloadAction);
};

// We want to be sure that we're compiled with large file support on linux,
// just in case we find ourselves downloading large images.
static_assert(8 == sizeof(off_t), "off_t not 64 bit");

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DOWNLOAD_ACTION_H__
