// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_ACTION_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_ACTION_H__

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <string>

#include <base/scoped_ptr.h>
#include <curl/curl.h>

#include "update_engine/action.h"
#include "update_engine/http_fetcher.h"

// The Omaha Request action makes a request to Omaha and can output
// the response on the output ActionPipe.

namespace chromeos_update_engine {

// Encodes XML entities in a given string with libxml2. input must be
// UTF-8 formatted. Output will be UTF-8 formatted.
std::string XmlEncode(const std::string& input);

// This struct encapsulates the data Omaha's response for the request.
// These strings in this struct are not XML escaped.
struct OmahaResponse {
  OmahaResponse()
      : update_exists(false),
        poll_interval(0),
        size(0),
        needs_admin(false),
        prompt(false) {}
  // True iff there is an update to be downloaded.
  bool update_exists;

  // If non-zero, server-dictated poll frequency in seconds.
  int poll_interval;

  // These are only valid if update_exists is true:
  std::string display_version;
  std::string codebase;
  std::string more_info_url;
  std::string hash;
  std::string deadline;
  off_t size;
  bool needs_admin;
  bool prompt;
  bool is_delta;
};
COMPILE_ASSERT(sizeof(off_t) == 8, off_t_not_64bit);

// This struct encapsulates the Omaha event information. For a
// complete list of defined event types and results, see
// http://code.google.com/p/omaha/wiki/ServerProtocol#event
struct OmahaEvent {
  enum Type {
    kTypeUnknown = 0,
    kTypeDownloadComplete = 1,
    kTypeInstallComplete = 2,
    kTypeUpdateComplete = 3,
    kTypeUpdateDownloadStarted = 13,
    kTypeUpdateDownloadFinished = 14,
  };

  enum Result {
    kResultError = 0,
    kResultSuccess = 1,
    kResultSuccessReboot = 2,
  };

  OmahaEvent()
      : type(kTypeUnknown),
        result(kResultError),
        error_code(kActionCodeError) {}
  explicit OmahaEvent(Type in_type)
      : type(in_type),
        result(kResultSuccess),
        error_code(kActionCodeSuccess) {}
  OmahaEvent(Type in_type, Result in_result, ActionExitCode in_error_code)
      : type(in_type),
        result(in_result),
        error_code(in_error_code) {}

  Type type;
  Result result;
  ActionExitCode error_code;
};

class NoneType;
class OmahaRequestAction;
struct OmahaRequestParams;
class PrefsInterface;

template<>
class ActionTraits<OmahaRequestAction> {
 public:
  // Takes parameters on the input pipe.
  typedef NoneType InputObjectType;
  // On UpdateCheck success, puts the Omaha response on output. Event
  // requests do not have an output pipe.
  typedef OmahaResponse OutputObjectType;
};

class OmahaRequestAction : public Action<OmahaRequestAction>,
                           public HttpFetcherDelegate {
 public:
  static const int kNeverPinged = -1;
  static const int kPingTimeJump = -2;

  // The ctor takes in all the parameters that will be used for making
  // the request to Omaha. For some of them we have constants that
  // should be used.
  //
  // Takes ownership of the passed in HttpFetcher. Useful for testing.
  //
  // Takes ownership of the passed in OmahaEvent. If |event| is NULL,
  // this is an UpdateCheck request, otherwise it's an Event request.
  // Event requests always succeed.
  //
  // A good calling pattern is:
  // OmahaRequestAction(..., new OmahaEvent(...), new WhateverHttpFetcher);
  // or
  // OmahaRequestAction(..., NULL, new WhateverHttpFetcher);
  OmahaRequestAction(PrefsInterface* prefs,
                     const OmahaRequestParams& params,
                     OmahaEvent* event,
                     HttpFetcher* http_fetcher,
                     bool ping_only);
  virtual ~OmahaRequestAction();
  typedef ActionTraits<OmahaRequestAction>::InputObjectType InputObjectType;
  typedef ActionTraits<OmahaRequestAction>::OutputObjectType OutputObjectType;
  void PerformAction();
  void TerminateProcessing();

  int GetHTTPResponseCode() { return http_fetcher_->http_response_code(); }

  // Debugging/logging
  static std::string StaticType() { return "OmahaRequestAction"; }
  std::string Type() const { return StaticType(); }

  // Delegate methods (see http_fetcher.h)
  virtual void ReceivedBytes(HttpFetcher *fetcher,
                             const char* bytes, int length);
  virtual void TransferComplete(HttpFetcher *fetcher, bool successful);

  // Returns true if this is an Event request, false if it's an UpdateCheck.
  bool IsEvent() const { return event_.get() != NULL; }

  void set_should_skip(bool should_skip) { should_skip_ = should_skip; }

 private:
  // If this is an update check request, initializes
  // |ping_active_days_| and |ping_roll_call_days_| to values that may
  // be sent as pings to Omaha.
  void InitPingDays();

  // Based on the persistent preference store values, calculates the
  // number of days since the last ping sent for |key|.
  int CalculatePingDays(const std::string& key);

  // Access to the preferences store.
  PrefsInterface* prefs_;

  // These are data that are passed in the request to the Omaha server.
  const OmahaRequestParams& params_;

  // Pointer to the OmahaEvent info. This is an UpdateCheck request if NULL.
  scoped_ptr<OmahaEvent> event_;

  // pointer to the HttpFetcher that does the http work
  scoped_ptr<HttpFetcher> http_fetcher_;

  // If true, only include the <ping> element in the request.
  bool ping_only_;

  // Stores the response from the omaha server
  std::vector<char> response_buffer_;

  // Initialized by InitPingDays to values that may be sent to Omaha
  // as part of a ping message. Note that only positive values and -1
  // are sent to Omaha.
  int ping_active_days_;
  int ping_roll_call_days_;

  // If true, this action should be a noop.
  bool should_skip_;

  DISALLOW_COPY_AND_ASSIGN(OmahaRequestAction);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_ACTION_H__
