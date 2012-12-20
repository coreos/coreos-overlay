// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glib.h>

#include "base/stringprintf.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "update_engine/omaha_request_action.h"
#include "update_engine/payload_state.h"
#include "update_engine/prefs_mock.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::SetArgumentPointee;

namespace chromeos_update_engine {

static void SetupPayloadStateWith2Urls(string hash,
                                       PayloadState* payload_state,
                                       OmahaResponse* response) {
  response->payload_urls.clear();
  response->payload_urls.push_back("http://test");
  response->payload_urls.push_back("https://test");
  response->size = 523456789;
  response->hash = hash;
  response->metadata_size = 558123;
  response->metadata_signature = "metasign";
  response->max_failure_count_per_url = 3;
  payload_state->SetResponse(*response);
  string stored_response = payload_state->GetResponse();
  string expected_response = StringPrintf(
      "NumURLs = 2\n"
      "Url0 = http://test\n"
      "Url1 = https://test\n"
      "Payload Size = 523456789\n"
      "Payload Sha256 Hash = %s\n"
      "Metadata Size = 558123\n"
      "Metadata Signature = metasign\n",
      hash.c_str());
  EXPECT_EQ(expected_response, stored_response);
}

class PayloadStateTest : public ::testing::Test { };

TEST(PayloadStateTest, DidYouAddANewActionExitCode) {
  if (kActionCodeUmaReportedMax != 40) {
    LOG(ERROR) << "The following failure is intentional. If you added a new "
               << "ActionExitCode enum value, make sure to add it to the "
               << "PayloadState::UpdateFailed method and then update this test "
               << "to the new value of kActionCodeUmaReportedMax, which is "
               << kActionCodeUmaReportedMax;
    EXPECT_FALSE("Please see the log line above");
  }
}

TEST(PayloadStateTest, SetResponseWorksWithEmptyResponse) {
  OmahaResponse response;
  NiceMock<PrefsMock> prefs;
  EXPECT_CALL(prefs, SetInt64(kPrefsPayloadAttemptNumber, 0));
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlIndex, 0));
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlFailureCount, 0));
  PayloadState payload_state;
  EXPECT_TRUE(payload_state.Initialize(&prefs));
  payload_state.SetResponse(response);
  string stored_response = payload_state.GetResponse();
  string expected_response = "NumURLs = 0\n"
                             "Payload Size = 0\n"
                             "Payload Sha256 Hash = \n"
                             "Metadata Size = 0\n"
                             "Metadata Signature = \n";
  EXPECT_EQ(expected_response, stored_response);
  EXPECT_EQ(0, payload_state.GetUrlIndex());
  EXPECT_EQ(0, payload_state.GetUrlFailureCount());
}

TEST(PayloadStateTest, SetResponseWorksWithSingleUrl) {
  OmahaResponse response;
  response.payload_urls.push_back("http://single.url.test");
  response.size = 123456789;
  response.hash = "hash";
  response.metadata_size = 58123;
  response.metadata_signature = "msign";
  NiceMock<PrefsMock> prefs;
  EXPECT_CALL(prefs, SetInt64(kPrefsPayloadAttemptNumber, 0));
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlIndex, 0));
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlFailureCount, 0));
  PayloadState payload_state;
  EXPECT_TRUE(payload_state.Initialize(&prefs));
  payload_state.SetResponse(response);
  string stored_response = payload_state.GetResponse();
  string expected_response = "NumURLs = 1\n"
                             "Url0 = http://single.url.test\n"
                             "Payload Size = 123456789\n"
                             "Payload Sha256 Hash = hash\n"
                             "Metadata Size = 58123\n"
                             "Metadata Signature = msign\n";
  EXPECT_EQ(expected_response, stored_response);
  EXPECT_EQ(0, payload_state.GetUrlIndex());
  EXPECT_EQ(0, payload_state.GetUrlFailureCount());
}

TEST(PayloadStateTest, SetResponseWorksWithMultipleUrls) {
  OmahaResponse response;
  response.payload_urls.push_back("http://multiple.url.test");
  response.payload_urls.push_back("https://multiple.url.test");
  response.size = 523456789;
  response.hash = "rhash";
  response.metadata_size = 558123;
  response.metadata_signature = "metasign";
  NiceMock<PrefsMock> prefs;
  EXPECT_CALL(prefs, SetInt64(kPrefsPayloadAttemptNumber, 0));
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlIndex, 0));
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlFailureCount, 0));
  PayloadState payload_state;
  EXPECT_TRUE(payload_state.Initialize(&prefs));
  payload_state.SetResponse(response);
  string stored_response = payload_state.GetResponse();
  string expected_response = "NumURLs = 2\n"
                             "Url0 = http://multiple.url.test\n"
                             "Url1 = https://multiple.url.test\n"
                             "Payload Size = 523456789\n"
                             "Payload Sha256 Hash = rhash\n"
                             "Metadata Size = 558123\n"
                             "Metadata Signature = metasign\n";
  EXPECT_EQ(expected_response, stored_response);
  EXPECT_EQ(0, payload_state.GetUrlIndex());
  EXPECT_EQ(0, payload_state.GetUrlFailureCount());
}

TEST(PayloadStateTest, CanAdvanceUrlIndexCorrectly) {
  OmahaResponse response;
  NiceMock<PrefsMock> prefs;
  PayloadState payload_state;

  // Payload attempt should start with 0 and then advance to 1.
  EXPECT_CALL(prefs, SetInt64(kPrefsPayloadAttemptNumber, 0)).Times(1);
  EXPECT_CALL(prefs, SetInt64(kPrefsPayloadAttemptNumber, 1)).Times(1);

  // Url index should go from 0 to 1 twice.
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlIndex, 0)).Times(2);
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlIndex, 1)).Times(2);

  // Failure count should be called each times url index is set, so that's
  // 4 times for this test.
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlFailureCount, 0)).Times(4);

  EXPECT_TRUE(payload_state.Initialize(&prefs));

  // This does a SetResponse which causes all the states to be set to 0 for
  // the first time.
  SetupPayloadStateWith2Urls("Hash1235", &payload_state, &response);
  EXPECT_EQ(0, payload_state.GetUrlIndex());

  // Verify that on the first error, the URL index advances to 1.
  ActionExitCode error = kActionCodeDownloadMetadataSignatureMismatch;
  payload_state.UpdateFailed(error);
  EXPECT_EQ(1, payload_state.GetUrlIndex());

  // Verify that on the next error, the URL index wraps around to 0.
  payload_state.UpdateFailed(error);
  EXPECT_EQ(0, payload_state.GetUrlIndex());

  // Verify that on the next error, it again advances to 1.
  payload_state.UpdateFailed(error);
  EXPECT_EQ(1, payload_state.GetUrlIndex());
}

TEST(PayloadStateTest, NewResponseResetsPayloadState) {
  OmahaResponse response;
  NiceMock<PrefsMock> prefs;
  PayloadState payload_state;

  EXPECT_TRUE(payload_state.Initialize(&prefs));

  // Set the first response.
  SetupPayloadStateWith2Urls("Hash5823", &payload_state, &response);

  // Advance the URL index to 1 by faking an error.
  ActionExitCode error = kActionCodeDownloadMetadataSignatureMismatch;
  payload_state.UpdateFailed(error);
  EXPECT_EQ(1, payload_state.GetUrlIndex());

  // Now, slightly change the response and set it again.
  SetupPayloadStateWith2Urls("Hash8225", &payload_state, &response);

  // Make sure the url index was reset to 0 because of the new response.
  EXPECT_EQ(0, payload_state.GetUrlIndex());
  EXPECT_EQ(0, payload_state.GetUrlFailureCount());
}

TEST(PayloadStateTest, AllCountersGetUpdatedProperlyOnErrorCodesAndEvents) {
  OmahaResponse response;
  PayloadState payload_state;
  NiceMock<PrefsMock> prefs;

  EXPECT_CALL(prefs, SetInt64(kPrefsPayloadAttemptNumber, 0)).Times(2);
  EXPECT_CALL(prefs, SetInt64(kPrefsPayloadAttemptNumber, 1)).Times(1);
  EXPECT_CALL(prefs, SetInt64(kPrefsPayloadAttemptNumber, 2)).Times(1);

  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlIndex, 0)).Times(4);
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlIndex, 1)).Times(2);

  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlFailureCount, 0)).Times(7);
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlFailureCount, 1)).Times(2);
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlFailureCount, 2)).Times(1);

  EXPECT_TRUE(payload_state.Initialize(&prefs));

  SetupPayloadStateWith2Urls("Hash5873", &payload_state, &response);

  // This should advance the URL index.
  payload_state.UpdateFailed(kActionCodeDownloadMetadataSignatureMismatch);
  EXPECT_EQ(0, payload_state.GetPayloadAttemptNumber());
  EXPECT_EQ(1, payload_state.GetUrlIndex());
  EXPECT_EQ(0, payload_state.GetUrlFailureCount());

  // This should advance the failure count only.
  payload_state.UpdateFailed(kActionCodeDownloadTransferError);
  EXPECT_EQ(0, payload_state.GetPayloadAttemptNumber());
  EXPECT_EQ(1, payload_state.GetUrlIndex());
  EXPECT_EQ(1, payload_state.GetUrlFailureCount());

  // This should advance the failure count only.
  payload_state.UpdateFailed(kActionCodeDownloadTransferError);
  EXPECT_EQ(0, payload_state.GetPayloadAttemptNumber());
  EXPECT_EQ(1, payload_state.GetUrlIndex());
  EXPECT_EQ(2, payload_state.GetUrlFailureCount());

  // This should advance the URL index as we've reached the
  // max failure count and reset the failure count for the new URL index.
  // This should also wrap around the URL index and thus cause the payload
  // attempt number to be incremented.
  payload_state.UpdateFailed(kActionCodeDownloadTransferError);
  EXPECT_EQ(1, payload_state.GetPayloadAttemptNumber());
  EXPECT_EQ(0, payload_state.GetUrlIndex());
  EXPECT_EQ(0, payload_state.GetUrlFailureCount());

  // This should advance the URL index.
  payload_state.UpdateFailed(kActionCodePayloadHashMismatchError);
  EXPECT_EQ(1, payload_state.GetPayloadAttemptNumber());
  EXPECT_EQ(1, payload_state.GetUrlIndex());
  EXPECT_EQ(0, payload_state.GetUrlFailureCount());

  // This should advance the URL index and payload attempt number due to
  // wrap-around of URL index.
  payload_state.UpdateFailed(kActionCodeDownloadMetadataSignatureMissingError);
  EXPECT_EQ(2, payload_state.GetPayloadAttemptNumber());
  EXPECT_EQ(0, payload_state.GetUrlIndex());
  EXPECT_EQ(0, payload_state.GetUrlFailureCount());

  // This HTTP error code should only increase the failure count.
  payload_state.UpdateFailed(static_cast<ActionExitCode>(
      kActionCodeOmahaRequestHTTPResponseBase + 404));
  EXPECT_EQ(2, payload_state.GetPayloadAttemptNumber());
  EXPECT_EQ(0, payload_state.GetUrlIndex());
  EXPECT_EQ(1, payload_state.GetUrlFailureCount());

  // And that failure count should be reset when we download some bytes
  // afterwards.
  payload_state.DownloadProgress(100);
  EXPECT_EQ(2, payload_state.GetPayloadAttemptNumber());
  EXPECT_EQ(0, payload_state.GetUrlIndex());
  EXPECT_EQ(0, payload_state.GetUrlFailureCount());

  // Now, slightly change the response and set it again.
  SetupPayloadStateWith2Urls("Hash8532", &payload_state, &response);

  // Make sure the url index was reset to 0 because of the new response.
  EXPECT_EQ(0, payload_state.GetPayloadAttemptNumber());
  EXPECT_EQ(0, payload_state.GetUrlIndex());
  EXPECT_EQ(0, payload_state.GetUrlFailureCount());
}

TEST(PayloadStateTest, PayloadAttemptNumberIncreasesOnSuccessfulDownload) {
  OmahaResponse response;
  PayloadState payload_state;
  NiceMock<PrefsMock> prefs;

  EXPECT_CALL(prefs, SetInt64(kPrefsPayloadAttemptNumber, 0)).Times(1);
  EXPECT_CALL(prefs, SetInt64(kPrefsPayloadAttemptNumber, 1)).Times(1);

  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlIndex, 0)).Times(1);
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlFailureCount, 0)).Times(1);

  EXPECT_TRUE(payload_state.Initialize(&prefs));

  SetupPayloadStateWith2Urls("Hash8593", &payload_state, &response);

  // This should just advance the payload attempt number;
  EXPECT_EQ(0, payload_state.GetPayloadAttemptNumber());
  payload_state.DownloadComplete();
  EXPECT_EQ(1, payload_state.GetPayloadAttemptNumber());
  EXPECT_EQ(0, payload_state.GetUrlIndex());
  EXPECT_EQ(0, payload_state.GetUrlFailureCount());
}

TEST(PayloadStateTest, SetResponseResetsInvalidUrlIndex) {
  OmahaResponse response;
  PayloadState payload_state;
  NiceMock<PrefsMock> prefs;

  EXPECT_TRUE(payload_state.Initialize(&prefs));
  SetupPayloadStateWith2Urls("Hash4427", &payload_state, &response);

  // Generate enough events to advance URL index, failure count and
  // payload attempt number all to 1.
  payload_state.DownloadComplete();
  payload_state.UpdateFailed(kActionCodeDownloadMetadataSignatureMismatch);
  payload_state.UpdateFailed(kActionCodeDownloadTransferError);
  EXPECT_EQ(1, payload_state.GetPayloadAttemptNumber());
  EXPECT_EQ(1, payload_state.GetUrlIndex());
  EXPECT_EQ(1, payload_state.GetUrlFailureCount());

  // Now, simulate a corrupted url index on persisted store which gets
  // loaded when update_engine restarts. Using a different prefs object
  // so as to not bother accounting for the uninteresting calls above.
  NiceMock<PrefsMock> prefs2;
  EXPECT_CALL(prefs2, Exists(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(prefs2, GetInt64(kPrefsPayloadAttemptNumber, _));
  EXPECT_CALL(prefs2, GetInt64(kPrefsCurrentUrlIndex, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(2), Return(true)));
  EXPECT_CALL(prefs2, GetInt64(kPrefsCurrentUrlFailureCount, _));

  // Note: This will be a different payload object, but the response should
  // have the same hash as before so as to not trivially reset because the
  // response was different. We want to specifically test that even if the
  // response is same, we should reset the state if we find it corrupted.
  EXPECT_TRUE(payload_state.Initialize(&prefs2));
  SetupPayloadStateWith2Urls("Hash4427", &payload_state, &response);

  // Make sure all counters get reset to 0 because of the corrupted URL index
  // we supplied above.
  EXPECT_EQ(0, payload_state.GetPayloadAttemptNumber());
  EXPECT_EQ(0, payload_state.GetUrlIndex());
  EXPECT_EQ(0, payload_state.GetUrlFailureCount());
}
}
