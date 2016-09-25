// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_state.h"

#include <algorithm>

#include <glog/logging.h>

#include "strings/string_printf.h"
#include "update_engine/prefs.h"
#include "update_engine/utils.h"

using std::chrono::system_clock;
using std::min;
using std::string;
using strings::StringPrintf;

namespace chromeos_update_engine {

// We want to upperbound backoffs to 16 days
static const uint32_t kMaxBackoffDays = 16;

// We want to randomize retry attempts after the backoff by +/- 6 hours.
static const uint32_t kMaxBackoffFuzzMinutes = 12 * 60;

bool PayloadState::Initialize(PrefsInterface* prefs) {
  CHECK(prefs);
  prefs_ = prefs;
  LoadResponseSignature();
  LoadPayloadAttemptNumber();
  LoadUrlIndex();
  LoadUrlFailureCount();
  LoadBackoffExpiryTime();
  return true;
}

void PayloadState::SetResponse(const OmahaResponse& omaha_response) {
  // Always store the latest response.
  response_ = omaha_response;

  // Check if the "signature" of this response (i.e. the fields we care about)
  // has changed.
  string new_response_signature = CalculateResponseSignature();
  bool has_response_changed = (response_signature_ != new_response_signature);

  // If the response has changed, we should persist the new signature and
  // clear away all the existing state.
  if (has_response_changed) {
    LOG(INFO) << "Resetting all persisted state as this is a new response";
    SetResponseSignature(new_response_signature);
    ResetPersistedState();
    return;
  }

  // This is the earliest point at which we can validate whether the URL index
  // we loaded from the persisted state is a valid value. If the response
  // hasn't changed but the URL index is invalid, it's indicative of some
  // tampering of the persisted state.
  if (url_index_ >= GetNumUrls()) {
    LOG(INFO) << "Resetting all payload state as the url index seems to have "
                 "been tampered with";
    ResetPersistedState();
    return;
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
  LOG(INFO) << "Updating payload state for error code: " << base_error
            << " (" << utils::CodeToString(base_error) << ")";

  if (GetNumUrls() == 0) {
    // This means we got this error even before we got a valid Omaha response.
    // So we should not advance the url_index_ in such cases.
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
    // of the backoff at the next update check.
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
    case kActionCodeOmahaUpdateDeferredForBackoff:
    case kActionCodePostinstallPowerwashError:
    case kActionCodeNewPCRPolicyVerificationError:
      LOG(INFO) << "Not incrementing URL index or failure count for this error";
      break;

    case kActionCodeSuccess:                            // success code
    case kActionCodeSetBootableFlagError:               // unused
    case kActionCodeOmahaRequestHTTPResponseBase:       // aggregated already
    case kActionCodeDevModeFlag:                       // not an error code
    case kActionCodeResumedFlag:                        // not an error code
    case kActionCodeTestImageFlag:                      // not an error code
    case kActionCodeTestOmahaUrlFlag:                   // not an error code
    case kActionCodeDownloadIncomplete:                 // not an error code
    case kSpecialFlags:                                 // not an error code
      // These shouldn't happen. Enumerating these  explicitly here so that we
      // can let the compiler warn about new error codes that are added to
      // action_processor.h but not added here.
      LOG(WARNING) << "Unexpected error code for UpdateFailed";
      break;

    // Note: Not adding a default here so as to let the compiler warn us of
    // any new enums that were added in the .h but not listed in this switch.
  }
}

bool PayloadState::ShouldBackoffDownload() {
  if (response_.disable_payload_backoff) {
    LOG(INFO) << "Payload backoff logic is disabled. "
                 "Can proceed with the download";
    return false;
  }

  if (response_.is_delta_payload) {
    // If delta payloads fail, we want to fallback quickly to full payloads as
    // they are more likely to succeed. Exponential backoffs would greatly
    // slow down the fallback to full payloads.  So we don't backoff for delta
    // payloads.
    LOG(INFO) << "No backoffs for delta payloads. "
              << "Can proceed with the download";
    return false;
  }

  if (!utils::IsOfficialBuild()) {
    // Backoffs are needed only for official builds. We do not want any delays
    // or update failures due to backoffs during testing or development.
    LOG(INFO) << "No backoffs for test/dev images. "
              << "Can proceed with the download";
    return false;
  }

  const auto since_epoch = backoff_expiry_time_.time_since_epoch();
  if (since_epoch == since_epoch.zero()) {
    LOG(INFO) << "No backoff expiry time has been set. "
              << "Can proceed with the download";
    return false;
  }

  if (backoff_expiry_time_ < system_clock::now()) {
    LOG(INFO) << "The backoff expiry time ("
              << utils::ToString(backoff_expiry_time_)
              << ") has elapsed. Can proceed with the download";
    return false;
  }

  LOG(INFO) << "Cannot proceed with downloads as we need to backoff until "
            << utils::ToString(backoff_expiry_time_);
  return true;
}

void PayloadState::IncrementPayloadAttemptNumber() {
  if (response_.is_delta_payload) {
    LOG(INFO) << "Not incrementing payload attempt number for delta payloads";
    return;
  }

  LOG(INFO) << "Incrementing the payload attempt number";
  SetPayloadAttemptNumber(GetPayloadAttemptNumber() + 1);
  UpdateBackoffExpiryTime();
}

void PayloadState::IncrementUrlIndex() {
  uint32_t next_url_index = GetUrlIndex() + 1;
  if (next_url_index < GetNumUrls()) {
    LOG(INFO) << "Incrementing the URL index for next attempt";
    SetUrlIndex(next_url_index);
  } else {
    LOG(INFO) << "Resetting the current URL index (" << GetUrlIndex() << ") to "
              << "0 as we only have " << GetNumUrls() << " URL(s)";
    SetUrlIndex(0);
    IncrementPayloadAttemptNumber();
  }

  // Whenever we update the URL index, we should also clear the URL failure
  // count so we can start over fresh for the new URL.
  SetUrlFailureCount(0);
}

void PayloadState::IncrementFailureCount() {
  uint32_t next_url_failure_count = GetUrlFailureCount() + 1;
  if (next_url_failure_count < response_.max_failure_count_per_url) {
    LOG(INFO) << "Incrementing the URL failure count";
    SetUrlFailureCount(next_url_failure_count);
  } else {
    LOG(INFO) << "Reached max number of failures for Url" << GetUrlIndex()
              << ". Trying next available URL";
    IncrementUrlIndex();
  }
}

void PayloadState::UpdateBackoffExpiryTime() {
  if (response_.disable_payload_backoff) {
    LOG(INFO) << "Resetting backoff expiry time as payload backoff is disabled";
    SetBackoffExpiryTime(system_clock::time_point());
    return;
  }

  if (GetPayloadAttemptNumber() == 0) {
    SetBackoffExpiryTime(system_clock::time_point());
    return;
  }

  // Since we're doing left-shift below, make sure we don't shift more
  // than this. E.g. if uint32_t is 4-bytes, don't left-shift more than 30 bits,
  // since we don't expect value of kMaxBackoffDays to be more than 100 anyway.
  uint32_t num_days = 1; // the value to be shifted.
  const uint32_t kMaxShifts = (sizeof(num_days) * 8) - 2;

  // Normal backoff days is 2 raised to (payload_attempt_number - 1).
  // E.g. if payload_attempt_number is over 30, limit power to 30.
  uint32_t power = min(GetPayloadAttemptNumber() - 1, kMaxShifts);

  // The number of days is the minimum of 2 raised to (payload_attempt_number
  // - 1) or kMaxBackoffDays.
  num_days = min(num_days << power, kMaxBackoffDays);

  // We don't want all retries to happen exactly at the same time when
  // retrying after backoff. So add some random minutes to fuzz.
  int fuzz_minutes = utils::FuzzInt(0, kMaxBackoffFuzzMinutes);
  std::chrono::minutes next_backoff_interval(num_days*24*60 + fuzz_minutes);
  LOG(INFO) << "Incrementing the backoff expiry time by "
            << utils::ToString(next_backoff_interval);
  SetBackoffExpiryTime(system_clock::now() + next_backoff_interval);
}

void PayloadState::ResetPersistedState() {
  SetPayloadAttemptNumber(0);
  SetUrlIndex(0);
  SetUrlFailureCount(0);
  UpdateBackoffExpiryTime(); // This will reset the backoff expiry time.
}

string PayloadState::CalculateResponseSignature() {
  string response_sign = StringPrintf("NumURLs = %zu\n",
                                      response_.payload_urls.size());

  for (size_t i = 0; i < response_.payload_urls.size(); i++)
    response_sign += StringPrintf("Url%zu = %s\n",
                                  i, response_.payload_urls[i].c_str());

  response_sign += StringPrintf("Payload Size = %jd\n"
                                "Payload Sha256 Hash = %s\n"
                                "Is Delta Payload = %d\n"
                                "Max Failure Count Per Url = %d\n"
                                "Disable Payload Backoff = %d\n",
                                static_cast<intmax_t>(response_.size),
                                response_.hash.c_str(),
                                response_.is_delta_payload,
                                response_.max_failure_count_per_url,
                                response_.disable_payload_backoff);
  return response_sign;
}

void PayloadState::LoadResponseSignature() {
  CHECK(prefs_);
  string stored_value;
  if (prefs_->Exists(kPrefsCurrentResponseSignature) &&
      prefs_->GetString(kPrefsCurrentResponseSignature, &stored_value)) {
    SetResponseSignature(stored_value);
  }
}

void PayloadState::SetResponseSignature(string response_signature) {
  CHECK(prefs_);
  response_signature_ = response_signature;
  LOG(INFO) << "Current Response Signature = \n" << response_signature_;
  prefs_->SetString(kPrefsCurrentResponseSignature, response_signature_);
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
    SetPayloadAttemptNumber(stored_value);
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
    // We only check for basic sanity value here. Detailed check will be
    // done in SetResponse once the first response comes in.
    if (stored_value < 0) {
      LOG(ERROR) << "Invalid URL Index (" << stored_value
                 << ") in persisted state. Defaulting to 0";
      stored_value = 0;
    }
    SetUrlIndex(stored_value);
  }
}

void PayloadState::SetUrlIndex(uint32_t url_index) {
  CHECK(prefs_);
  url_index_ = url_index;
  LOG(INFO) << "Current URL Index = " << url_index_;
  prefs_->SetInt64(kPrefsCurrentUrlIndex, url_index_);
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
    SetUrlFailureCount(stored_value);
  }
}

void PayloadState::SetUrlFailureCount(uint32_t url_failure_count) {
  CHECK(prefs_);
  url_failure_count_ = url_failure_count;
  LOG(INFO) << "Current URL (Url" << GetUrlIndex()
            << ")'s Failure Count = " << url_failure_count_;
  prefs_->SetInt64(kPrefsCurrentUrlFailureCount, url_failure_count_);
}

void PayloadState::LoadBackoffExpiryTime() {
  CHECK(prefs_);
  int64_t stored_value;
  if (!prefs_->Exists(kPrefsBackoffExpiryTime))
    return;

  if (!prefs_->GetInt64(kPrefsBackoffExpiryTime, &stored_value))
    return;

  auto stored_time = system_clock::from_time_t(stored_value);
  if (stored_time > system_clock::now() + utils::days_t(kMaxBackoffDays)) {
    LOG(ERROR) << "Invalid backoff expiry time ("
               << utils::ToString(stored_time)
               << ") in persisted state. Resetting.";
    stored_time = system_clock::now();
  }
  SetBackoffExpiryTime(stored_time);
}

void PayloadState::SetBackoffExpiryTime(const system_clock::time_point& new_time) {
  CHECK(prefs_);
  backoff_expiry_time_ = new_time;
  LOG(INFO) << "Backoff Expiry Time = "
            << utils::ToString(backoff_expiry_time_);
  prefs_->SetInt64(kPrefsBackoffExpiryTime,
                   system_clock::to_time_t(backoff_expiry_time_));
}

}  // namespace chromeos_update_engine
