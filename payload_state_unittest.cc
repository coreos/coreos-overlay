// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glib.h>

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "update_engine/omaha_request_action.h"
#include "update_engine/payload_state.h"
#include "update_engine/prefs_mock.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using testing::_;
using testing::NiceMock;
using testing::SetArgumentPointee;

namespace chromeos_update_engine {

class PayloadStateTest : public ::testing::Test { };

TEST(PayloadStateTest, SetResponseWorksWithEmptyResponse) {
  OmahaResponse response;
  NiceMock<PrefsMock> prefs;
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlIndex, 0));
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
}

TEST(PayloadStateTest, SetResponseWorksWithSingleUrl) {
  OmahaResponse response;
  response.payload_urls.push_back("http://single.url.test");
  response.size = 123456789;
  response.hash = "hash";
  response.metadata_size = 58123;
  response.metadata_signature = "msign";
  NiceMock<PrefsMock> prefs;
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlIndex, 0));
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
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlIndex, 0));
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
}

TEST(PayloadStateTest, CanAdvanceUrlIndexCorrectly) {
  OmahaResponse response;
  // For a variation, don't set the metadata signature, as it's optional.
  response.payload_urls.push_back("http://set.url.index");
  response.payload_urls.push_back("https://set.url.index");
  response.size = 523456789;
  response.hash = "rhash";

  NiceMock<PrefsMock> prefs;
  // Should be called 4 times, one for each UpdateFailed call plus one
  // for SetResponse.
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlIndex, _)).Times(4);
  PayloadState payload_state;
  EXPECT_TRUE(payload_state.Initialize(&prefs));
  payload_state.SetResponse(response);
  string stored_response = payload_state.GetResponse();
  string expected_response = "NumURLs = 2\n"
                             "Url0 = http://set.url.index\n"
                             "Url1 = https://set.url.index\n"
                             "Payload Size = 523456789\n"
                             "Payload Sha256 Hash = rhash\n"
                             "Metadata Size = 0\n"
                             "Metadata Signature = \n";
  EXPECT_EQ(expected_response, stored_response);
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
  response.payload_urls.push_back("http://reset.url.index");
  response.payload_urls.push_back("https://reset.url.index");
  response.size = 523456789;
  response.hash = "rhash";
  response.metadata_size = 558123;
  response.metadata_signature = "metasign";
  NiceMock<PrefsMock> prefs;
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlIndex, 0)).Times(2);
  EXPECT_CALL(prefs, SetInt64(kPrefsCurrentUrlIndex, 1)).Times(1);
  PayloadState payload_state;
  EXPECT_TRUE(payload_state.Initialize(&prefs));
  payload_state.SetResponse(response);
  string stored_response = payload_state.GetResponse();
  string expected_response = "NumURLs = 2\n"
                             "Url0 = http://reset.url.index\n"
                             "Url1 = https://reset.url.index\n"
                             "Payload Size = 523456789\n"
                             "Payload Sha256 Hash = rhash\n"
                             "Metadata Size = 558123\n"
                             "Metadata Signature = metasign\n";
  EXPECT_EQ(expected_response, stored_response);

  // Advance the URL index to 1 by faking an error.
  ActionExitCode error = kActionCodeDownloadMetadataSignatureMismatch;
  payload_state.UpdateFailed(error);
  EXPECT_EQ(1, payload_state.GetUrlIndex());

  // Now, slightly change the response and set it again.
  response.hash = "newhash";
  payload_state.SetResponse(response);
  stored_response = payload_state.GetResponse();
  expected_response = "NumURLs = 2\n"
                      "Url0 = http://reset.url.index\n"
                      "Url1 = https://reset.url.index\n"
                      "Payload Size = 523456789\n"
                      "Payload Sha256 Hash = newhash\n"
                      "Metadata Size = 558123\n"
                      "Metadata Signature = metasign\n";
  // Verify the new response.
  EXPECT_EQ(expected_response, stored_response);

  // Make sure the url index was reset to 0 because of the new response.
  EXPECT_EQ(0, payload_state.GetUrlIndex());
}

}
