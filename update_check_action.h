// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_UPDATE_CHECK_ACTION_H__
#define UPDATE_ENGINE_UPDATE_CHECK_ACTION_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>

#include <curl/curl.h>

#include "base/scoped_ptr.h"
#include "action.h"
#include "http_fetcher.h"

// The Update Check action makes an update check request to Omaha and
// can output the response on the output ActionPipe.

using std::string;

namespace chromeos_update_engine {

// Encodes XML entities in a given string with libxml2. input must be
// UTF-8 formatted. Output will be UTF-8 formatted.
std::string XmlEncode(const std::string& input);

// This struct encapsulates the data Omaha gets for the update check.
// These strings in this struct should not be XML escaped.
struct UpdateCheckParams {
  UpdateCheckParams()
      : os_platform(kOsPlatform), os_version(kOsVersion), app_id(kAppId) {}
  UpdateCheckParams(const std::string& in_machine_id,
                    const std::string& in_user_id,
                    const std::string& in_os_platform,
                    const std::string& in_os_version,
                    const std::string& in_os_sp,
                    const std::string& in_app_id,
                    const std::string& in_app_version,
                    const std::string& in_app_lang,
                    const std::string& in_app_track)
      : machine_id(in_machine_id),
        user_id(in_user_id),
        os_platform(in_os_platform),
        os_version(in_os_version),
        os_sp(in_os_sp),
        app_id(in_app_id),
        app_version(in_app_version),
        app_lang(in_app_lang),
        app_track(in_app_track) {}

    std::string machine_id;
    std::string user_id;
    std::string os_platform;
    std::string os_version;
    std::string os_sp;
    std::string app_id;
    std::string app_version;
    std::string app_lang;
    std::string app_track;

  // Suggested defaults
  static const char* const kAppId;
  static const char* const kOsPlatform;
  static const char* const kOsVersion;
};

// This struct encapsulates the data Omaha returns for the update check.
// These strings in this struct are not XML escaped.
struct UpdateCheckResponse {
  UpdateCheckResponse()
      : update_exists(false), size(0), needs_admin(false), prompt(false) {}
  // True iff there is an update to be downloaded.
  bool update_exists;

  // These are only valid if update_exists is true:
  std::string display_version;
  std::string codebase;
  std::string more_info_url;
  std::string hash;
  off_t size;
  bool needs_admin;
  bool prompt;
};
COMPILE_ASSERT(sizeof(off_t) == 8, off_t_not_64bit);

class UpdateCheckAction;
class NoneType;

template<>
class ActionTraits<UpdateCheckAction> {
 public:
  // Does not take an object for input
  typedef NoneType InputObjectType;
  // On success, puts the output path on output
  typedef UpdateCheckResponse OutputObjectType;
};

class UpdateCheckAction : public Action<UpdateCheckAction>,
                          public HttpFetcherDelegate {
 public:
  // The ctor takes in all the parameters that will be used for
  // making the request to Omaha. For some of them we have constants
  // that should be used.
  // Takes ownership of the passed in HttpFetcher. Useful for testing.
  // A good calling pattern is:
  // UpdateCheckAction(..., new WhateverHttpFetcher);
  UpdateCheckAction(const UpdateCheckParams& params,
                    HttpFetcher* http_fetcher);
  virtual ~UpdateCheckAction();
  typedef ActionTraits<UpdateCheckAction>::InputObjectType InputObjectType;
  typedef ActionTraits<UpdateCheckAction>::OutputObjectType OutputObjectType;
  void PerformAction();
  void TerminateProcessing();

  // Debugging/logging
  std::string Type() const { return "UpdateCheckAction"; }

  // Delegate methods (see http_fetcher.h)
  virtual void ReceivedBytes(HttpFetcher *fetcher,
                             const char* bytes, int length);
  virtual void TransferComplete(HttpFetcher *fetcher, bool successful);

 private:
  // These are data that are passed in the request to the Omaha server
  UpdateCheckParams params_;

  // pointer to the HttpFetcher that does the http work
  scoped_ptr<HttpFetcher> http_fetcher_;

  // Stores the response from the omaha server
  std::vector<char> response_buffer_;

  DISALLOW_COPY_AND_ASSIGN(UpdateCheckAction);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_UPDATE_CHECK_ACTION_H__
