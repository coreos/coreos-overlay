// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_state.h"

#include <base/logging.h>
#include <base/stringprintf.h>

#include "update_engine/omaha_request_action.h"
#include "update_engine/prefs.h"
#include "update_engine/utils.h"

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
  LoadPayloadAttemptNumber();
  LoadUrlIndex();
  LoadUrlFailureCount();
  LogPayloadState();
  return true;
}

void PayloadState::SetResponse(const OmahaResponse& omaha_response) {
  CHECK(prefs_);
  num_urls_ = omaha_response.payload_urls.size();
  max_failure_count_per_url_ = omaha_response.max_failure_count_per_url;
  string new_response = GetFilteredResponse(omaha_response);
  bool has_response_changed = (response_ != new_response);
  response_ = new_response;
  LOG(INFO) << "Stored Response = \n" << response_;
  prefs_->SetString(kPrefsCurrentResponse, response_);

  bool should_reset = false;
  if (has_response_changed) {
    LOG(INFO) << "Resetting all payload state as this is a new response";
    should_reset = true;
  } else if (url_index_ >= num_urls_) {
    LOG(INFO) << "Resetting all payload state as the persisted state "
              << "seems to have been tampered with";
    should_reset = true;
  }

  if (should_reset) {
    SetPayloadAttemptNumber(0);
    SetUrlIndex(0);
  }
}

void PayloadState::DownloadComplete() {
  LOG(INFO) << "Payload downloaded successfully";
  IncrementPayloadAttemptNumber();
}

void PayloadState::DownloadProgress(size_t count) {
  if (count == 0)
    return;

  // We've received non-zero bytes from a recent download operation.  Since our
  // URL failure count is meant to penalize a URL only for consecutive
  // failures, downloading bytes successfully means we should reset the failure
  // count (as we know at least that the URL is working). In future, we can
  // design this to be more sophisticated to check for more intelligent failure
  // patterns, but right now, even 1 byte downloaded will mark the URL to be
  // good unless it hits 10 (or configured number of) consecutive failures
  // again.

  if (GetUrlFailureCount() == 0)
    return;

  LOG(INFO) << "Resetting failure count of Url" << GetUrlIndex()
            << " to 0 as we received " << count << " bytes successfully";
  SetUrlFailureCount(0);
}

void PayloadState::UpdateFailed(ActionExitCode error) {
  ActionExitCode base_error = utils::GetBaseErrorCode(error);
  LOG(INFO) << "Updating payload state for error code: " << base_error;

  if (!num_urls_) {
    // Since we don't persist num_urls_, it's possible that we get an error in
    // our communication to Omaha before even OmahaRequestAction code had a
    // chance to call SetResponse (which sets num_urls_). So we should not
    // advance the url_index_ in such cases.
    LOG(INFO) << "Ignoring failures until we get a valid Omaha response.";
    return;
  }

  switch (base_error) {
    // Errors which are good indicators of a problem with a particular URL or
    // the protocol used in the URL or entities in the communication channel
    // (e.g. proxies). We should try the next available URL in the next update
    // check to quickly recover from these errors.
    case kActionCodePayloadHashMismatchError:
    case kActionCodePayloadSizeMismatchError:
    case kActionCodeDownloadPayloadVerificationError:
    case kActionCodeDownloadPayloadPubKeyVerificationError:
    case kActionCodeSignedDeltaPayloadExpectedError:
    case kActionCodeDownloadInvalidMetadataMagicString:
    case kActionCodeDownloadSignatureMissingInManifest:
    case kActionCodeDownloadManifestParseError:
    case kActionCodeDownloadMetadataSignatureError:
    case kActionCodeDownloadMetadataSignatureVerificationError:
    case kActionCodeDownloadMetadataSignatureMismatch:
    case kActionCodeDownloadOperationHashVerificationError:
    case kActionCodeDownloadOperationExecutionError:
    case kActionCodeDownloadOperationHashMismatch:
    case kActionCodeDownloadInvalidMetadataSize:
    case kActionCodeDownloadInvalidMetadataSignature:
    case kActionCodeDownloadOperationHashMissingError:
    case kActionCodeDownloadMetadataSignatureMissingError:
      IncrementUrlIndex();
      break;

    // Errors which seem to be just transient network/communication related
    // failures and do not indicate any inherent problem with the URL itself.
    // So, we should keep the current URL but just increment the
    // failure count to give it more chances. This way, while we maximize our
    // chances of downloading from the URLs that appear earlier in the response
    // (because download from a local server URL that appears earlier in a
    // response is preferable than downloading from the next URL which could be
    // a internet URL and thus could be more expensive).
    case kActionCodeError:
    case kActionCodeDownloadTransferError:
    case kActionCodeDownloadWriteError:
    case kActionCodeDownloadStateInitializationError:
    case kActionCodeOmahaErrorInHTTPResponse: // Aggregate code for HTTP errors.
      IncrementFailureCount();
      break;

    // Errors which are not specific to a URL and hence shouldn't result in
    // the URL being penalized. This can happen in two cases:
    // 1. We haven't started downloading anything: These errors don't cost us
    // anything in terms of actual payload bytes, so we should just do the
    // regular retries at the next update check.
    // 2. We have successfully downloaded the payload: In this case, the
    // payload attempt number would have been incremented and would take care
    // of the back-off at the next update check.
    // In either case, there's no need to update URL index or failure count.
    case kActionCodeOmahaRequestError:
    case kActionCodeOmahaResponseHandlerError:
    case kActionCodePostinstallRunnerError:
    case kActionCodeFilesystemCopierError:
    case kActionCodeInstallDeviceOpenError:
    case kActionCodeKernelDeviceOpenError:
    case kActionCodeDownloadNewPartitionInfoError:
    case kActionCodeNewRootfsVerificationError:
    case kActionCodeNewKernelVerificationError:
    case kActionCodePostinstallBootedFromFirmwareB:
    case kActionCodeOmahaRequestEmptyResponseError:
    case kActionCodeOmahaRequestXMLParseError:
    case kActionCodeOmahaResponseInvalid:
    case kActionCodeOmahaUpdateIgnoredPerPolicy:
    case kActionCodeOmahaUpdateDeferredPerPolicy:
      LOG(INFO) << "Not incrementing URL index or failure count for this error";
      break;

    case kActionCodeSuccess:                            // success code
    case kActionCodeSetBootableFlagError:               // unused
    case kActionCodeUmaReportedMax:                     // not an error code
    case kActionCodeOmahaRequestHTTPResponseBase:       // aggregated already
    case kActionCodeResumedFlag:                        // not an error code
    case kActionCodeBootModeFlag:                       // not an error code
    case kActualCodeMask:                               // not an error code
      // These shouldn't happen. Enumerating these  explicitly here so that we
      // can let the compiler warn about new error codes that are added to
      // action_processor.h but not added here.
      LOG(WARNING) << "Unexpected error code for UpdateFailed";
      break;

    // Note: Not adding a default here so as to let the compiler warn us of
    // any new enums that were added in the .h but not listed in this switch.
  }
}

void PayloadState::LogPayloadState() {
  LOG(INFO) << "Current Payload State:\n"
            << "Current Response = \n" << response_
            << "\nPayload Attempt Number = " << payload_attempt_number_
            << "\nCurrent URL Index = " << url_index_
            << "\nCurrent URL Failure Count = " << url_failure_count_;
}

void PayloadState::IncrementPayloadAttemptNumber() {
  LOG(INFO) << "Incrementing the payload attempt number";
  SetPayloadAttemptNumber(GetPayloadAttemptNumber() + 1);

  // TODO(jaysri): chromium-os:36806: Implement the payload back-off logic
  // that uses the payload attempt number.
}

void PayloadState::IncrementUrlIndex() {
  uint32_t next_url_index = GetUrlIndex() + 1;
  if (next_url_index < num_urls_) {
    LOG(INFO) << "Incrementing the URL index for next attempt";
    SetUrlIndex(next_url_index);
  } else {
    LOG(INFO) << "Resetting the current URL index (" << GetUrlIndex() << ") to "
              << "0 as we only have " << num_urls_ << " URL(s)";
    SetUrlIndex(0);
    IncrementPayloadAttemptNumber();
  }
}

void PayloadState::IncrementFailureCount() {
  uint32_t next_url_failure_count = GetUrlFailureCount() + 1;
  if (next_url_failure_count < max_failure_count_per_url_) {
    LOG(INFO) << "Incrementing the URL failure count";
    SetUrlFailureCount(next_url_failure_count);
  } else {
    LOG(INFO) << "Reached max number of failures for Url" << GetUrlIndex()
              << ". Trying next available URL";
    IncrementUrlIndex();
  }
}

void PayloadState::LoadResponse() {
  CHECK(prefs_);
  string stored_value;
  if (prefs_->Exists(kPrefsCurrentResponse) &&
      prefs_->GetString(kPrefsCurrentResponse, &stored_value)) {
    response_ = stored_value;
  }
}

void PayloadState::LoadPayloadAttemptNumber() {
  CHECK(prefs_);
  int64_t stored_value;
  if (prefs_->Exists(kPrefsPayloadAttemptNumber) &&
      prefs_->GetInt64(kPrefsPayloadAttemptNumber, &stored_value)) {
    if (stored_value < 0) {
      LOG(ERROR) << "Invalid payload attempt number (" << stored_value
                 << ") in persisted state. Defaulting to 0";
      stored_value = 0;
    }
    payload_attempt_number_ = stored_value;
  }
}

void PayloadState::SetPayloadAttemptNumber(uint32_t payload_attempt_number) {
  CHECK(prefs_);
  payload_attempt_number_ = payload_attempt_number;
  LOG(INFO) << "Payload Attempt Number = " << payload_attempt_number_;
  prefs_->SetInt64(kPrefsPayloadAttemptNumber, payload_attempt_number_);
}

void PayloadState::LoadUrlIndex() {
  CHECK(prefs_);
  int64_t stored_value;
  if (prefs_->Exists(kPrefsCurrentUrlIndex) &&
      prefs_->GetInt64(kPrefsCurrentUrlIndex, &stored_value)) {
    if (stored_value < 0) {
      LOG(ERROR) << "Invalid URL Index (" << stored_value
                 << ") in persisted state. Defaulting to 0";
      stored_value = 0;
    }
    url_index_ = stored_value;
  }
}

void PayloadState::SetUrlIndex(uint32_t url_index) {
  CHECK(prefs_);
  url_index_ = url_index;
  LOG(INFO) << "Current URL Index = " << url_index_;
  prefs_->SetInt64(kPrefsCurrentUrlIndex, url_index_);

  // Everytime we set the URL index, we should also reset its failure count.
  // Otherwise, the URL will be tried only once, instead of
  // max_failure_count_per_url times in the next round.
  SetUrlFailureCount(0);
}

void PayloadState::LoadUrlFailureCount() {
  CHECK(prefs_);
  int64_t stored_value;
  if (prefs_->Exists(kPrefsCurrentUrlFailureCount) &&
      prefs_->GetInt64(kPrefsCurrentUrlFailureCount, &stored_value)) {
    if (stored_value < 0) {
      LOG(ERROR) << "Invalid URL Failure count (" << stored_value
                 << ") in persisted state. Defaulting to 0";
      stored_value = 0;
    }
    url_failure_count_ = stored_value;
  }
}

void PayloadState::SetUrlFailureCount(uint32_t url_failure_count) {
  CHECK(prefs_);
  url_failure_count_ = url_failure_count;
  LOG(INFO) << "Current URL (Url" << GetUrlIndex()
            << ")'s Failure Count = " << url_failure_count_;
  prefs_->SetInt64(kPrefsCurrentUrlFailureCount, url_failure_count_);
}

}  // namespace chromeos_update_engine
