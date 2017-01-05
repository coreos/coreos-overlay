// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_ACTION_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_ACTION_H__

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <memory>
#include <string>
#include <vector>

#include <curl/curl.h>
#include <libxml/parser.h>

#include "update_engine/action.h"
#include "update_engine/http_fetcher.h"
#include "update_engine/system_state.h"
#include "update_engine/omaha_response.h"
#include "update_engine/utils.h"

// The Omaha Request action makes a request to Omaha and can output
// the response on the output ActionPipe.

namespace chromeos_update_engine {

// Encodes XML entities in a given string with libxml2. input must be
// UTF-8 formatted. Output will be UTF-8 formatted.
std::string XmlEncode(const std::string& input);

// This struct encapsulates the Omaha event information. For a
// complete list of defined event types and results, see
// https://github.com/google/omaha/blob/wiki/ServerProtocol.md#event-element
struct OmahaEvent {
  // The Type values correspond to EVENT_TYPE values of Omaha.
  enum Type {
    kTypeUnknown = 0,
    kTypeDownloadComplete = 1,
    kTypeInstallComplete = 2,
    kTypeUpdateComplete = 3,
    kTypeUpdateDownloadStarted = 13,
    kTypeUpdateDownloadFinished = 14,
  };

  // The Result values correspond to EVENT_RESULT values of Omaha.
  enum Result {
    kResultError = 0,
    kResultSuccess = 1,
    kResultSuccessReboot = 2,
    kResultUpdateDeferred = 9, // When we ignore/defer updates due to policy.
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
  // We choose this value of 10 as a heuristic for a work day in trying
  // each URL, assuming we check roughly every 45 mins. This is a good time to
  // wait - neither too long nor too little - so we don't give up the preferred
  // URLs that appear earlier in list too quickly before moving on to the
  // fallback ones.
  static const int kDefaultMaxFailureCountPerUrl = 10;

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
  OmahaRequestAction(SystemState* system_state,
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

 private:
  // Parses the response from Omaha that's available in |doc| using the other
  // helper methods below and populates the |output_object| with the relevant
  // values. Returns true if we should continue the parsing.  False otherwise,
  // in which case it sets any error code using |completer|.
  bool ParseResponse(xmlDoc* doc,
                     OmahaResponse* output_object,
                     ScopedActionCompleter* completer);

  // Parses the status property in the given update_check_node and populates
  // |output_object| if valid. Returns true if we should continue the parsing.
  // False otherwise, in which case it sets any error code using |completer|.
  bool ParseStatus(xmlNode* update_check_node,
                   OmahaResponse* output_object,
                   ScopedActionCompleter* completer);

  // Parses the version attribute on manifest XML node and populates
  // |output_object| if valid. Returns true if we should continue the parsing.
  // False otherwise, in which case it sets any error code using |completer|.
  bool ParseManifest(xmlDoc* doc,
                     OmahaResponse* output_object,
                     ScopedActionCompleter* completer);

  // Parses the URL nodes in the given XML document and populates
  // |output_object| if valid. Returns true if we should continue the parsing.
  // False otherwise, in which case it sets any error code using |completer|.
  bool ParseUrls(xmlDoc* doc,
                 OmahaResponse* output_object,
                 ScopedActionCompleter* completer);

  // Parses the package node in the given XML document and populates
  // |output_object| if valid. Returns true if we should continue the parsing.
  // False otherwise, in which case it sets any error code using |completer|.
  bool ParsePackage(xmlDoc* doc,
                    OmahaResponse* output_object,
                    ScopedActionCompleter* completer);

  // Parses the other parameters in the given XML document and populates
  // |output_object| if valid. Returns true if we should continue the parsing.
  // False otherwise, in which case it sets any error code using |completer|.
  bool ParseParams(xmlDoc* doc,
                   OmahaResponse* output_object,
                   ScopedActionCompleter* completer);

  // Global system context.
  SystemState* system_state_;

  // Contains state that is relevant in the processing of the Omaha request.
  OmahaRequestParams* params_;

  // Pointer to the OmahaEvent info. This is an UpdateCheck request if NULL.
  std::unique_ptr<OmahaEvent> event_;

  // pointer to the HttpFetcher that does the http work
  std::unique_ptr<HttpFetcher> http_fetcher_;

  // If true, only include the <ping> element in the request.
  bool ping_only_;

  // Stores the response from the omaha server
  std::vector<char> response_buffer_;

  // Initialized by InitPingDays to values that may be sent to Omaha
  // as part of a ping message. Note that only positive values and -1
  // are sent to Omaha.
  int ping_active_days_;
  int ping_roll_call_days_;

  DISALLOW_COPY_AND_ASSIGN(OmahaRequestAction);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_ACTION_H__
