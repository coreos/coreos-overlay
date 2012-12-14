// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_state.h"

#include <base/logging.h>
#include <base/stringprintf.h>

#include "update_engine/omaha_request_action.h"
#include "update_engine/prefs.h"

using std::string;

namespace chromeos_update_engine {

// Returns a string containing that subset of the fields from the OmahaResponse
// which we're interested in persisting for the purpose of detecting whether
// we should clear the rest of the payload state when we get a new
// OmahaResponse.
static string GetFilteredResponse(const OmahaResponse& response) {
  string mini_response = StringPrintf("NumURLs = %d\n",
                                      response.payload_urls.size());

  for (size_t i = 0; i < response.payload_urls.size(); i++)
    mini_response += StringPrintf("Url%d = %s\n",
                                  i, response.payload_urls[i].c_str());

  mini_response += StringPrintf("Payload Size = %llu\n"
                                "Payload Sha256 Hash = %s\n"
                                "Metadata Size = %llu\n"
                                "Metadata Signature = %s\n",
                                response.size,
                                response.hash.c_str(),
                                response.metadata_size,
                                response.metadata_signature.c_str());
  return mini_response;
}

bool PayloadState::Initialize(PrefsInterface* prefs) {
  CHECK(prefs);
  prefs_ = prefs;
  LoadResponse();
  LoadUrlIndex();
  LogPayloadState();
  return true;
}

void PayloadState::LogPayloadState() {
  LOG(INFO) << "Current Payload State:\n"
            << "Current Response = \n" << response_
            << "Current URL Index = " << url_index_;
}

void PayloadState::SetResponse(const OmahaResponse& omaha_response) {
  CHECK(prefs_);
  num_urls_ = omaha_response.payload_urls.size();
  string new_response = GetFilteredResponse(omaha_response);
  bool has_response_changed = (response_ != new_response);
  response_ = new_response;
  LOG(INFO) << "Stored Response = \n" << response_;
  prefs_->SetString(kPrefsCurrentResponse, response_);

  if (has_response_changed) {
    LOG(INFO) << "Resetting all payload state as this is a new response";
    SetUrlIndex(0);
  }
}

void PayloadState::UpdateFailed(ActionExitCode error) {
  LOG(INFO) << "Updating payload state for error code: " << error;

  if (!num_urls_) {
    // Since we don't persist num_urls_, it's possible that we get an error in
    // our communication to Omaha before even OmahaRequestAction code had a
    // chance to call SetResponse (which sets num_urls_). So we should not
    // advance the url_index_ in such cases.
    LOG(INFO) << "Ignoring failures until we get a valid Omaha response.";
    return;
  }

  // chromium-os:37206: Classify the errors and advance the URL index at
  // different rates for different errors in the next CL. Until then, advance
  // URL index on every single error.
  uint32_t next_url_index = GetUrlIndex() + 1;
  if (next_url_index < num_urls_) {
    LOG(INFO) << "Advancing the URL index for next attempt";
  } else {
    LOG(INFO) << "Resetting the current URL index (" << GetUrlIndex() << ") to "
              << "0 as we only have " << num_urls_ << " URL(s)";
    next_url_index = 0;

    // TODO(jaysri): This is the place where we should increment the
    // payload_attempt_number so that we can back-off appropriately.
  }

  SetUrlIndex(next_url_index);
}

string PayloadState::LoadResponse() {
  CHECK(prefs_);
  string stored_value;
  if (prefs_->Exists(kPrefsCurrentResponse) &&
      prefs_->GetString(kPrefsCurrentResponse, &stored_value)) {
    response_ = stored_value;
  }
  return response_;
}

uint32_t PayloadState::LoadUrlIndex() {
  CHECK(prefs_);
  int64_t stored_value;
  if (prefs_->Exists(kPrefsCurrentUrlIndex) &&
      prefs_->GetInt64(kPrefsCurrentUrlIndex, &stored_value)) {
    url_index_ = stored_value;
  }
  return url_index_;
}

void PayloadState::SetUrlIndex(uint32_t url_index) {
  CHECK(prefs_);
  // TODO(jaysri): When we implement failure count, make sure to reset
  // the failure count when url index changes.
  url_index_ = url_index;
  LOG(INFO) << "Current URL Index = " << url_index_;
  prefs_->SetInt64(kPrefsCurrentUrlIndex, url_index_);
}

}  // namespace chromeos_update_engine
