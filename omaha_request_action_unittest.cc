// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <glib.h>

#include "base/string_util.h"
#include "base/time.h"
#include "gtest/gtest.h"
#include "update_engine/action_pipe.h"
#include "update_engine/mock_http_fetcher.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/omaha_request_action.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/prefs_mock.h"
#include "update_engine/test_utils.h"

using base::Time;
using base::TimeDelta;
using std::string;
using std::vector;
using testing::_;
using testing::AllOf;
using testing::Ge;
using testing::Le;
using testing::NiceMock;
using testing::Return;
using testing::SetArgumentPointee;

namespace chromeos_update_engine {

class OmahaRequestActionTest : public ::testing::Test { };

namespace {
const OmahaRequestParams kDefaultTestParams(
    OmahaRequestParams::kOsPlatform,
    OmahaRequestParams::kOsVersion,
    "service_pack",
    "x86-generic",
    OmahaRequestParams::kAppId,
    "0.1.0.0",
    "en-US",
    "unittest",
    "OEM MODEL 09235 7471",
    false,  // delta okay
    "http://url");

string GetNoUpdateResponse(const string& app_id) {
  return string(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><gupdate "
      "xmlns=\"http://www.google.com/update2/response\" protocol=\"2.0\"><app "
      "appid=\"") + app_id + "\" status=\"ok\"><ping "
      "status=\"ok\"/><updatecheck status=\"noupdate\"/></app></gupdate>";
}

string GetUpdateResponse(const string& app_id,
                         const string& display_version,
                         const string& more_info_url,
                         const string& prompt,
                         const string& codebase,
                         const string& hash,
                         const string& needsadmin,
                         const string& size,
                         const string& deadline) {
  return string("<?xml version=\"1.0\" encoding=\"UTF-8\"?><gupdate "
                "xmlns=\"http://www.google.com/update2/response\" "
                "protocol=\"2.0\"><app "
                "appid=\"") + app_id + "\" status=\"ok\"><ping "
      "status=\"ok\"/><updatecheck DisplayVersion=\"" + display_version + "\" "
      "MoreInfo=\"" + more_info_url + "\" Prompt=\"" + prompt + "\" "
      "IsDelta=\"true\" "
      "codebase=\"" + codebase + "\" hash=\"not-applicable\" "
      "sha256=\"" + hash + "\" needsadmin=\"" + needsadmin + "\" "
      "size=\"" + size + "\" deadline=\"" + deadline +
      "\" status=\"ok\"/></app></gupdate>";
}

class OmahaRequestActionTestProcessorDelegate : public ActionProcessorDelegate {
 public:
  OmahaRequestActionTestProcessorDelegate()
      : loop_(NULL),
        expected_code_(kActionCodeSuccess) {}
  virtual ~OmahaRequestActionTestProcessorDelegate() {
  }
  virtual void ProcessingDone(const ActionProcessor* processor,
                              ActionExitCode code) {
    ASSERT_TRUE(loop_);
    g_main_loop_quit(loop_);
  }

  virtual void ActionCompleted(ActionProcessor* processor,
                               AbstractAction* action,
                               ActionExitCode code) {
    // make sure actions always succeed
    if (action->Type() == OmahaRequestAction::StaticType())
      EXPECT_EQ(expected_code_, code);
    else
      EXPECT_EQ(kActionCodeSuccess, code);
  }
  GMainLoop *loop_;
  ActionExitCode expected_code_;
};

gboolean StartProcessorInRunLoop(gpointer data) {
  ActionProcessor *processor = reinterpret_cast<ActionProcessor*>(data);
  processor->StartProcessing();
  return FALSE;
}
}  // namespace {}

class OutputObjectCollectorAction;

template<>
class ActionTraits<OutputObjectCollectorAction> {
 public:
  // Does not take an object for input
  typedef OmahaResponse InputObjectType;
  // On success, puts the output path on output
  typedef NoneType OutputObjectType;
};

class OutputObjectCollectorAction : public Action<OutputObjectCollectorAction> {
 public:
  OutputObjectCollectorAction() : has_input_object_(false) {}
  void PerformAction() {
    // copy input object
    has_input_object_ = HasInputObject();
    if (has_input_object_)
      omaha_response_ = GetInputObject();
    processor_->ActionComplete(this, kActionCodeSuccess);
  }
  // Should never be called
  void TerminateProcessing() {
    CHECK(false);
  }
  // Debugging/logging
  static std::string StaticType() {
    return "OutputObjectCollectorAction";
  }
  std::string Type() const { return StaticType(); }
  bool has_input_object_;
  OmahaResponse omaha_response_;
};

// Returns true iff an output response was obtained from the
// OmahaRequestAction. |prefs| may be NULL, in which case a local PrefsMock is
// used. out_response may be NULL. If |fail_http_response_code| is
// non-negative, the transfer will fail with that code. out_post_data may be
// null; if non-null, the post-data received by the mock HttpFetcher is
// returned.
bool TestUpdateCheck(PrefsInterface* prefs,
                     const OmahaRequestParams& params,
                     const string& http_response,
                     int fail_http_response_code,
                     ActionExitCode expected_code,
                     OmahaResponse* out_response,
                     vector<char>* out_post_data) {
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  MockHttpFetcher* fetcher = new MockHttpFetcher(http_response.data(),
                                                 http_response.size());
  if (fail_http_response_code >= 0) {
    fetcher->FailTransfer(fail_http_response_code);
  }
  NiceMock<PrefsMock> local_prefs;
  OmahaRequestAction action(prefs ? prefs : &local_prefs,
                            params,
                            NULL,
                            fetcher);
  OmahaRequestActionTestProcessorDelegate delegate;
  delegate.loop_ = loop;
  delegate.expected_code_ = expected_code;

  ActionProcessor processor;
  processor.set_delegate(&delegate);
  processor.EnqueueAction(&action);

  OutputObjectCollectorAction collector_action;
  BondActions(&action, &collector_action);
  processor.EnqueueAction(&collector_action);

  g_timeout_add(0, &StartProcessorInRunLoop, &processor);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
  if (collector_action.has_input_object_ && out_response)
    *out_response = collector_action.omaha_response_;
  if (out_post_data)
    *out_post_data = fetcher->post_data();
  return collector_action.has_input_object_;
}

// Tests Event requests -- they should always succeed. |out_post_data|
// may be null; if non-null, the post-data received by the mock
// HttpFetcher is returned.
void TestEvent(const OmahaRequestParams& params,
               OmahaEvent* event,
               const string& http_response,
               vector<char>* out_post_data) {
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  MockHttpFetcher* fetcher = new MockHttpFetcher(http_response.data(),
                                                 http_response.size());
  NiceMock<PrefsMock> prefs;
  OmahaRequestAction action(&prefs, params, event, fetcher);
  OmahaRequestActionTestProcessorDelegate delegate;
  delegate.loop_ = loop;
  ActionProcessor processor;
  processor.set_delegate(&delegate);
  processor.EnqueueAction(&action);

  g_timeout_add(0, &StartProcessorInRunLoop, &processor);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
  if (out_post_data)
    *out_post_data = fetcher->post_data();
}

TEST(OmahaRequestActionTest, NoUpdateTest) {
  OmahaResponse response;
  ASSERT_TRUE(
      TestUpdateCheck(NULL,  // prefs
                      kDefaultTestParams,
                      GetNoUpdateResponse(OmahaRequestParams::kAppId),
                      -1,
                      kActionCodeSuccess,
                      &response,
                      NULL));
  EXPECT_FALSE(response.update_exists);
}

TEST(OmahaRequestActionTest, ValidUpdateTest) {
  OmahaResponse response;
  ASSERT_TRUE(
      TestUpdateCheck(NULL,  // prefs
                      kDefaultTestParams,
                      GetUpdateResponse(OmahaRequestParams::kAppId,
                                        "1.2.3.4",  // version
                                        "http://more/info",
                                        "true",  // prompt
                                        "http://code/base",  // dl url
                                        "HASH1234=",  // checksum
                                        "false",  // needs admin
                                        "123",  // size
                                        "20101020"),  // deadline
                      -1,
                      kActionCodeSuccess,
                      &response,
                      NULL));
  EXPECT_TRUE(response.update_exists);
  EXPECT_EQ("1.2.3.4", response.display_version);
  EXPECT_EQ("http://code/base", response.codebase);
  EXPECT_EQ("http://more/info", response.more_info_url);
  EXPECT_TRUE(response.is_delta);
  EXPECT_EQ("HASH1234=", response.hash);
  EXPECT_EQ(123, response.size);
  EXPECT_FALSE(response.needs_admin);
  EXPECT_TRUE(response.prompt);
  EXPECT_EQ("20101020", response.deadline);
}

TEST(OmahaRequestActionTest, NoOutputPipeTest) {
  const string http_response(GetNoUpdateResponse(OmahaRequestParams::kAppId));

  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);

  NiceMock<PrefsMock> prefs;
  OmahaRequestAction action(&prefs, kDefaultTestParams, NULL,
                            new MockHttpFetcher(http_response.data(),
                                                http_response.size()));
  OmahaRequestActionTestProcessorDelegate delegate;
  delegate.loop_ = loop;
  ActionProcessor processor;
  processor.set_delegate(&delegate);
  processor.EnqueueAction(&action);

  g_timeout_add(0, &StartProcessorInRunLoop, &processor);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
  EXPECT_FALSE(processor.IsRunning());
}

TEST(OmahaRequestActionTest, InvalidXmlTest) {
  OmahaResponse response;
  ASSERT_FALSE(
      TestUpdateCheck(NULL,  // prefs
                      kDefaultTestParams,
                      "invalid xml>",
                      -1,
                      kActionCodeOmahaRequestXMLParseError,
                      &response,
                      NULL));
  EXPECT_FALSE(response.update_exists);
}

TEST(OmahaRequestActionTest, EmptyResponseTest) {
  OmahaResponse response;
  ASSERT_FALSE(
      TestUpdateCheck(NULL,  // prefs
                      kDefaultTestParams,
                      "",
                      -1,
                      kActionCodeOmahaRequestEmptyResponseError,
                      &response,
                      NULL));
  EXPECT_FALSE(response.update_exists);
}

TEST(OmahaRequestActionTest, MissingStatusTest) {
  OmahaResponse response;
  ASSERT_FALSE(TestUpdateCheck(
      NULL,  // prefs
      kDefaultTestParams,
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><gupdate "
      "xmlns=\"http://www.google.com/update2/response\" protocol=\"2.0\"><app "
      "appid=\"foo\" status=\"ok\"><ping "
      "status=\"ok\"/><updatecheck/></app></gupdate>",
      -1,
      kActionCodeOmahaRequestNoUpdateCheckStatus,
      &response,
      NULL));
  EXPECT_FALSE(response.update_exists);
}

TEST(OmahaRequestActionTest, InvalidStatusTest) {
  OmahaResponse response;
  ASSERT_FALSE(TestUpdateCheck(
      NULL,  // prefs
      kDefaultTestParams,
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><gupdate "
      "xmlns=\"http://www.google.com/update2/response\" protocol=\"2.0\"><app "
      "appid=\"foo\" status=\"ok\"><ping "
      "status=\"ok\"/><updatecheck status=\"foo\"/></app></gupdate>",
      -1,
      kActionCodeOmahaRequestBadUpdateCheckStatus,
      &response,
      NULL));
  EXPECT_FALSE(response.update_exists);
}

TEST(OmahaRequestActionTest, MissingNodesetTest) {
  OmahaResponse response;
  ASSERT_FALSE(TestUpdateCheck(
      NULL,  // prefs
      kDefaultTestParams,
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><gupdate "
      "xmlns=\"http://www.google.com/update2/response\" protocol=\"2.0\"><app "
      "appid=\"foo\" status=\"ok\"><ping "
      "status=\"ok\"/></app></gupdate>",
      -1,
      kActionCodeOmahaRequestNoUpdateCheckNode,
      &response,
      NULL));
  EXPECT_FALSE(response.update_exists);
}

TEST(OmahaRequestActionTest, MissingFieldTest) {
  OmahaResponse response;
  ASSERT_TRUE(TestUpdateCheck(NULL,  // prefs
                              kDefaultTestParams,
                              string("<?xml version=\"1.0\" "
                                     "encoding=\"UTF-8\"?><gupdate "
                                     "xmlns=\"http://www.google.com/"
                                     "update2/response\" "
                                     "protocol=\"2.0\"><app appid=\"") +
                              OmahaRequestParams::kAppId
                              + "\" status=\"ok\"><ping "
                              "status=\"ok\"/><updatecheck "
                              "DisplayVersion=\"1.2.3.4\" "
                              "Prompt=\"false\" "
                              "codebase=\"http://code/base\" hash=\"foo\" "
                              "sha256=\"HASH1234=\" needsadmin=\"true\" "
                              "size=\"123\" "
                              "status=\"ok\"/></app></gupdate>",
                              -1,
                              kActionCodeSuccess,
                              &response,
                              NULL));
  EXPECT_TRUE(response.update_exists);
  EXPECT_EQ("1.2.3.4", response.display_version);
  EXPECT_EQ("http://code/base", response.codebase);
  EXPECT_EQ("", response.more_info_url);
  EXPECT_FALSE(response.is_delta);
  EXPECT_EQ("HASH1234=", response.hash);
  EXPECT_EQ(123, response.size);
  EXPECT_TRUE(response.needs_admin);
  EXPECT_FALSE(response.prompt);
  EXPECT_TRUE(response.deadline.empty());
}

namespace {
class TerminateEarlyTestProcessorDelegate : public ActionProcessorDelegate {
 public:
  void ProcessingStopped(const ActionProcessor* processor) {
    ASSERT_TRUE(loop_);
    g_main_loop_quit(loop_);
  }
  GMainLoop *loop_;
};

gboolean TerminateTransferTestStarter(gpointer data) {
  ActionProcessor *processor = reinterpret_cast<ActionProcessor*>(data);
  processor->StartProcessing();
  CHECK(processor->IsRunning());
  processor->StopProcessing();
  return FALSE;
}
}  // namespace {}

TEST(OmahaRequestActionTest, TerminateTransferTest) {
  string http_response("doesn't matter");
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);

  NiceMock<PrefsMock> prefs;
  OmahaRequestAction action(&prefs, kDefaultTestParams, NULL,
                            new MockHttpFetcher(http_response.data(),
                                                http_response.size()));
  TerminateEarlyTestProcessorDelegate delegate;
  delegate.loop_ = loop;
  ActionProcessor processor;
  processor.set_delegate(&delegate);
  processor.EnqueueAction(&action);

  g_timeout_add(0, &TerminateTransferTestStarter, &processor);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
}

TEST(OmahaRequestActionTest, XmlEncodeTest) {
  EXPECT_EQ("ab", XmlEncode("ab"));
  EXPECT_EQ("a&lt;b", XmlEncode("a<b"));
  EXPECT_EQ("foo-&#x3A9;", XmlEncode("foo-\xce\xa9"));
  EXPECT_EQ("&lt;&amp;&gt;", XmlEncode("<&>"));
  EXPECT_EQ("&amp;lt;&amp;amp;&amp;gt;", XmlEncode("&lt;&amp;&gt;"));

  vector<char> post_data;

  // Make sure XML Encode is being called on the params
  OmahaRequestParams params(OmahaRequestParams::kOsPlatform,
                            OmahaRequestParams::kOsVersion,
                            "testtheservice_pack>",
                            "x86 generic<id",
                            OmahaRequestParams::kAppId,
                            "0.1.0.0",
                            "en-US",
                            "unittest_track&lt;",
                            "<OEM MODEL>",
                            false,  // delta okay
                            "http://url");
  OmahaResponse response;
  ASSERT_FALSE(
      TestUpdateCheck(NULL,  // prefs
                      params,
                      "invalid xml>",
                      -1,
                      kActionCodeOmahaRequestXMLParseError,
                      &response,
                      &post_data));
  // convert post_data to string
  string post_str(&post_data[0], post_data.size());
  EXPECT_NE(post_str.find("testtheservice_pack&gt;"), string::npos);
  EXPECT_EQ(post_str.find("testtheservice_pack>"), string::npos);
  EXPECT_NE(post_str.find("x86 generic&lt;id"), string::npos);
  EXPECT_EQ(post_str.find("x86 generic<id"), string::npos);
  EXPECT_NE(post_str.find("unittest_track&amp;lt;"), string::npos);
  EXPECT_EQ(post_str.find("unittest_track&lt;"), string::npos);
  EXPECT_NE(post_str.find("&lt;OEM MODEL&gt;"), string::npos);
  EXPECT_EQ(post_str.find("<OEM MODEL>"), string::npos);
}

TEST(OmahaRequestActionTest, XmlDecodeTest) {
  OmahaResponse response;
  ASSERT_TRUE(
      TestUpdateCheck(NULL,  // prefs
                      kDefaultTestParams,
                      GetUpdateResponse(OmahaRequestParams::kAppId,
                                        "1.2.3.4",  // version
                                        "testthe&lt;url",  // more info
                                        "true",  // prompt
                                        "testthe&amp;codebase",  // dl url
                                        "HASH1234=", // checksum
                                        "false",  // needs admin
                                        "123",  // size
                                        "&lt;20110101"),  // deadline
                      -1,
                      kActionCodeSuccess,
                      &response,
                      NULL));

  EXPECT_EQ(response.more_info_url, "testthe<url");
  EXPECT_EQ(response.codebase, "testthe&codebase");
  EXPECT_EQ(response.deadline, "<20110101");
}

TEST(OmahaRequestActionTest, ParseIntTest) {
  OmahaResponse response;
  ASSERT_TRUE(
      TestUpdateCheck(NULL,  // prefs
                      kDefaultTestParams,
                      GetUpdateResponse(OmahaRequestParams::kAppId,
                                        "1.2.3.4",  // version
                                        "theurl",  // more info
                                        "true",  // prompt
                                        "thecodebase",  // dl url
                                        "HASH1234=", // checksum
                                        "false",  // needs admin
                                        // overflows int32:
                                        "123123123123123",  // size
                                        "deadline"),
                      -1,
                      kActionCodeSuccess,
                      &response,
                      NULL));

  EXPECT_EQ(response.size, 123123123123123ll);
}

TEST(OmahaRequestActionTest, FormatUpdateCheckOutputTest) {
  vector<char> post_data;
  ASSERT_FALSE(TestUpdateCheck(NULL,  // prefs
                               kDefaultTestParams,
                               "invalid xml>",
                               -1,
                               kActionCodeOmahaRequestXMLParseError,
                               NULL,  // response
                               &post_data));
  // convert post_data to string
  string post_str(&post_data[0], post_data.size());
  EXPECT_NE(post_str.find("        <o:ping a=\"-1\" r=\"-1\"></o:ping>\n"
                          "        <o:updatecheck></o:updatecheck>\n"),
            string::npos);
  EXPECT_NE(post_str.find("hardware_class=\"OEM MODEL 09235 7471\""),
            string::npos);
  EXPECT_EQ(post_str.find("o:event"), string::npos);
}

TEST(OmahaRequestActionTest, FormatSuccessEventOutputTest) {
  vector<char> post_data;
  TestEvent(kDefaultTestParams,
            new OmahaEvent(OmahaEvent::kTypeUpdateDownloadStarted),
            "invalid xml>",
            &post_data);
  // convert post_data to string
  string post_str(&post_data[0], post_data.size());
  string expected_event = StringPrintf(
      "        <o:event eventtype=\"%d\" eventresult=\"%d\"></o:event>\n",
      OmahaEvent::kTypeUpdateDownloadStarted,
      OmahaEvent::kResultSuccess);
  EXPECT_NE(post_str.find(expected_event), string::npos);
  EXPECT_EQ(post_str.find("o:ping"), string::npos);
  EXPECT_EQ(post_str.find("o:updatecheck"), string::npos);
}

TEST(OmahaRequestActionTest, FormatErrorEventOutputTest) {
  vector<char> post_data;
  TestEvent(kDefaultTestParams,
            new OmahaEvent(OmahaEvent::kTypeDownloadComplete,
                           OmahaEvent::kResultError,
                           kActionCodeError),
            "invalid xml>",
            &post_data);
  // convert post_data to string
  string post_str(&post_data[0], post_data.size());
  string expected_event = StringPrintf(
      "        <o:event eventtype=\"%d\" eventresult=\"%d\" "
      "errorcode=\"%d\"></o:event>\n",
      OmahaEvent::kTypeDownloadComplete,
      OmahaEvent::kResultError,
      kActionCodeError);
  EXPECT_NE(post_str.find(expected_event), string::npos);
  EXPECT_EQ(post_str.find("o:updatecheck"), string::npos);
}

TEST(OmahaRequestActionTest, FormatEventOutputTest) {
  vector<char> post_data;
  TestEvent(kDefaultTestParams,
            new OmahaEvent(OmahaEvent::kTypeDownloadComplete,
                           OmahaEvent::kResultError,
                           kActionCodeError),
            "invalid xml>",
            &post_data);
  // convert post_data to string
  string post_str(&post_data[0], post_data.size());
  string expected_event = StringPrintf(
      "        <o:event eventtype=\"%d\" eventresult=\"%d\" "
      "errorcode=\"%d\"></o:event>\n",
      OmahaEvent::kTypeDownloadComplete,
      OmahaEvent::kResultError,
      kActionCodeError);
  EXPECT_NE(post_str.find(expected_event), string::npos);
  EXPECT_EQ(post_str.find("o:updatecheck"), string::npos);
}

TEST(OmahaRequestActionTest, IsEventTest) {
  string http_response("doesn't matter");
  NiceMock<PrefsMock> prefs;
  OmahaRequestAction update_check_action(
      &prefs,
      kDefaultTestParams,
      NULL,
      new MockHttpFetcher(http_response.data(),
                          http_response.size()));
  EXPECT_FALSE(update_check_action.IsEvent());

  OmahaRequestAction event_action(
      &prefs,
      kDefaultTestParams,
      new OmahaEvent(OmahaEvent::kTypeUpdateComplete),
      new MockHttpFetcher(http_response.data(),
                          http_response.size()));
  EXPECT_TRUE(event_action.IsEvent());
}

TEST(OmahaRequestActionTest, FormatDeltaOkayOutputTest) {
  for (int i = 0; i < 2; i++) {
    bool delta_okay = i == 1;
    const char* delta_okay_str = delta_okay ? "true" : "false";
    vector<char> post_data;
    OmahaRequestParams params(OmahaRequestParams::kOsPlatform,
                              OmahaRequestParams::kOsVersion,
                              "service_pack",
                              "x86-generic",
                              OmahaRequestParams::kAppId,
                              "0.1.0.0",
                              "en-US",
                              "unittest_track",
                              "OEM MODEL REV 1234",
                              delta_okay,
                              "http://url");
    ASSERT_FALSE(TestUpdateCheck(NULL,  // prefs
                                 params,
                                 "invalid xml>",
                                 -1,
                                 kActionCodeOmahaRequestXMLParseError,
                                 NULL,
                                 &post_data));
    // convert post_data to string
    string post_str(&post_data[0], post_data.size());
    EXPECT_NE(post_str.find(StringPrintf(" delta_okay=\"%s\"", delta_okay_str)),
              string::npos)
        << "i = " << i;
  }
}

TEST(OmahaRequestActionTest, OmahaEventTest) {
  OmahaEvent default_event;
  EXPECT_EQ(OmahaEvent::kTypeUnknown, default_event.type);
  EXPECT_EQ(OmahaEvent::kResultError, default_event.result);
  EXPECT_EQ(kActionCodeError, default_event.error_code);

  OmahaEvent success_event(OmahaEvent::kTypeUpdateDownloadStarted);
  EXPECT_EQ(OmahaEvent::kTypeUpdateDownloadStarted, success_event.type);
  EXPECT_EQ(OmahaEvent::kResultSuccess, success_event.result);
  EXPECT_EQ(kActionCodeSuccess, success_event.error_code);

  OmahaEvent error_event(OmahaEvent::kTypeUpdateDownloadFinished,
                         OmahaEvent::kResultError,
                         kActionCodeError);
  EXPECT_EQ(OmahaEvent::kTypeUpdateDownloadFinished, error_event.type);
  EXPECT_EQ(OmahaEvent::kResultError, error_event.result);
  EXPECT_EQ(kActionCodeError, error_event.error_code);
}

TEST(OmahaRequestActionTest, PingTest) {
  NiceMock<PrefsMock> prefs;
  // Add a few hours to the day difference to test no rounding, etc.
  int64_t five_days_ago =
      (Time::Now() - TimeDelta::FromHours(5 * 24 + 13)).ToInternalValue();
  int64_t six_days_ago =
      (Time::Now() - TimeDelta::FromHours(6 * 24 + 11)).ToInternalValue();
  EXPECT_CALL(prefs, GetInt64(kPrefsLastActivePingDay, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(six_days_ago), Return(true)));
  EXPECT_CALL(prefs, GetInt64(kPrefsLastRollCallPingDay, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(five_days_ago), Return(true)));
  vector<char> post_data;
  ASSERT_TRUE(
      TestUpdateCheck(&prefs,
                      kDefaultTestParams,
                      GetNoUpdateResponse(OmahaRequestParams::kAppId),
                      -1,
                      kActionCodeSuccess,
                      NULL,
                      &post_data));
  string post_str(&post_data[0], post_data.size());
  EXPECT_NE(post_str.find("<o:ping a=\"6\" r=\"5\"></o:ping>"), string::npos);
}

TEST(OmahaRequestActionTest, ActivePingTest) {
  NiceMock<PrefsMock> prefs;
  int64_t three_days_ago =
      (Time::Now() - TimeDelta::FromHours(3 * 24 + 12)).ToInternalValue();
  int64_t now = Time::Now().ToInternalValue();
  EXPECT_CALL(prefs, GetInt64(kPrefsLastActivePingDay, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(three_days_ago), Return(true)));
  EXPECT_CALL(prefs, GetInt64(kPrefsLastRollCallPingDay, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(now), Return(true)));
  vector<char> post_data;
  ASSERT_TRUE(
      TestUpdateCheck(&prefs,
                      kDefaultTestParams,
                      GetNoUpdateResponse(OmahaRequestParams::kAppId),
                      -1,
                      kActionCodeSuccess,
                      NULL,
                      &post_data));
  string post_str(&post_data[0], post_data.size());
  EXPECT_NE(post_str.find("<o:ping a=\"3\"></o:ping>"), string::npos);
}

TEST(OmahaRequestActionTest, RollCallPingTest) {
  NiceMock<PrefsMock> prefs;
  int64_t four_days_ago =
      (Time::Now() - TimeDelta::FromHours(4 * 24)).ToInternalValue();
  int64_t now = Time::Now().ToInternalValue();
  EXPECT_CALL(prefs, GetInt64(kPrefsLastActivePingDay, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(now), Return(true)));
  EXPECT_CALL(prefs, GetInt64(kPrefsLastRollCallPingDay, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(four_days_ago), Return(true)));
  vector<char> post_data;
  ASSERT_TRUE(
      TestUpdateCheck(&prefs,
                      kDefaultTestParams,
                      GetNoUpdateResponse(OmahaRequestParams::kAppId),
                      -1,
                      kActionCodeSuccess,
                      NULL,
                      &post_data));
  string post_str(&post_data[0], post_data.size());
  EXPECT_NE(post_str.find("<o:ping r=\"4\"></o:ping>\n"), string::npos);
}

TEST(OmahaRequestActionTest, NoPingTest) {
  NiceMock<PrefsMock> prefs;
  int64_t one_hour_ago =
      (Time::Now() - TimeDelta::FromHours(1)).ToInternalValue();
  EXPECT_CALL(prefs, GetInt64(kPrefsLastActivePingDay, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(one_hour_ago), Return(true)));
  EXPECT_CALL(prefs, GetInt64(kPrefsLastRollCallPingDay, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(one_hour_ago), Return(true)));
  EXPECT_CALL(prefs, SetInt64(kPrefsLastActivePingDay, _)).Times(0);
  EXPECT_CALL(prefs, SetInt64(kPrefsLastRollCallPingDay, _)).Times(0);
  vector<char> post_data;
  ASSERT_TRUE(
      TestUpdateCheck(&prefs,
                      kDefaultTestParams,
                      GetNoUpdateResponse(OmahaRequestParams::kAppId),
                      -1,
                      kActionCodeSuccess,
                      NULL,
                      &post_data));
  string post_str(&post_data[0], post_data.size());
  EXPECT_EQ(post_str.find("o:ping"), string::npos);
}

TEST(OmahaRequestActionTest, BackInTimePingTest) {
  NiceMock<PrefsMock> prefs;
  int64_t future =
      (Time::Now() + TimeDelta::FromHours(3 * 24 + 4)).ToInternalValue();
  EXPECT_CALL(prefs, GetInt64(kPrefsLastActivePingDay, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(future), Return(true)));
  EXPECT_CALL(prefs, GetInt64(kPrefsLastRollCallPingDay, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(future), Return(true)));
  EXPECT_CALL(prefs, SetInt64(kPrefsLastActivePingDay, _))
      .WillOnce(Return(true));
  EXPECT_CALL(prefs, SetInt64(kPrefsLastRollCallPingDay, _))
      .WillOnce(Return(true));
  vector<char> post_data;
  ASSERT_TRUE(
      TestUpdateCheck(&prefs,
                      kDefaultTestParams,
                      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><gupdate "
                      "xmlns=\"http://www.google.com/update2/response\" "
                      "protocol=\"2.0\"><daystart elapsed_seconds=\"100\"/>"
                      "<app appid=\"foo\" status=\"ok\"><ping status=\"ok\"/>"
                      "<updatecheck status=\"noupdate\"/></app></gupdate>",
                      -1,
                      kActionCodeSuccess,
                      NULL,
                      &post_data));
  string post_str(&post_data[0], post_data.size());
  EXPECT_EQ(post_str.find("o:ping"), string::npos);
}

TEST(OmahaRequestActionTest, LastPingDayUpdateTest) {
  // This test checks that the action updates the last ping day to now
  // minus 200 seconds with a slack of 5 seconds. Therefore, the test
  // may fail if it runs for longer than 5 seconds. It shouldn't run
  // that long though.
  int64_t midnight =
      (Time::Now() - TimeDelta::FromSeconds(200)).ToInternalValue();
  int64_t midnight_slack =
      (Time::Now() - TimeDelta::FromSeconds(195)).ToInternalValue();
  NiceMock<PrefsMock> prefs;
  EXPECT_CALL(prefs, SetInt64(kPrefsLastActivePingDay,
                              AllOf(Ge(midnight), Le(midnight_slack))))
      .WillOnce(Return(true));
  EXPECT_CALL(prefs, SetInt64(kPrefsLastRollCallPingDay,
                              AllOf(Ge(midnight), Le(midnight_slack))))
      .WillOnce(Return(true));
  ASSERT_TRUE(
      TestUpdateCheck(&prefs,
                      kDefaultTestParams,
                      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><gupdate "
                      "xmlns=\"http://www.google.com/update2/response\" "
                      "protocol=\"2.0\"><daystart elapsed_seconds=\"200\"/>"
                      "<app appid=\"foo\" status=\"ok\"><ping status=\"ok\"/>"
                      "<updatecheck status=\"noupdate\"/></app></gupdate>",
                      -1,
                      kActionCodeSuccess,
                      NULL,
                      NULL));
}

TEST(OmahaRequestActionTest, NoElapsedSecondsTest) {
  NiceMock<PrefsMock> prefs;
  EXPECT_CALL(prefs, SetInt64(kPrefsLastActivePingDay, _)).Times(0);
  EXPECT_CALL(prefs, SetInt64(kPrefsLastRollCallPingDay, _)).Times(0);
  ASSERT_TRUE(
      TestUpdateCheck(&prefs,
                      kDefaultTestParams,
                      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><gupdate "
                      "xmlns=\"http://www.google.com/update2/response\" "
                      "protocol=\"2.0\"><daystart blah=\"200\"/>"
                      "<app appid=\"foo\" status=\"ok\"><ping status=\"ok\"/>"
                      "<updatecheck status=\"noupdate\"/></app></gupdate>",
                      -1,
                      kActionCodeSuccess,
                      NULL,
                      NULL));
}

TEST(OmahaRequestActionTest, BadElapsedSecondsTest) {
  NiceMock<PrefsMock> prefs;
  EXPECT_CALL(prefs, SetInt64(kPrefsLastActivePingDay, _)).Times(0);
  EXPECT_CALL(prefs, SetInt64(kPrefsLastRollCallPingDay, _)).Times(0);
  ASSERT_TRUE(
      TestUpdateCheck(&prefs,
                      kDefaultTestParams,
                      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><gupdate "
                      "xmlns=\"http://www.google.com/update2/response\" "
                      "protocol=\"2.0\"><daystart elapsed_seconds=\"x\"/>"
                      "<app appid=\"foo\" status=\"ok\"><ping status=\"ok\"/>"
                      "<updatecheck status=\"noupdate\"/></app></gupdate>",
                      -1,
                      kActionCodeSuccess,
                      NULL,
                      NULL));
}

TEST(OmahaRequestActionTest, NoUniqueIDTest) {
  vector<char> post_data;
  ASSERT_FALSE(TestUpdateCheck(NULL,  // prefs
                               kDefaultTestParams,
                               "invalid xml>",
                               -1,
                               kActionCodeOmahaRequestXMLParseError,
                               NULL,  // response
                               &post_data));
  // convert post_data to string
  string post_str(&post_data[0], post_data.size());
  EXPECT_EQ(post_str.find("machineid="), string::npos);
  EXPECT_EQ(post_str.find("userid="), string::npos);
}

TEST(OmahaRequestActionTest, NetworkFailureTest) {
  OmahaResponse response;
  ASSERT_FALSE(
      TestUpdateCheck(NULL,  // prefs
                      kDefaultTestParams,
                      "",
                      501,
                      static_cast<ActionExitCode>(
                          kActionCodeOmahaRequestHTTPResponseBase + 501),
                      &response,
                      NULL));
  EXPECT_FALSE(response.update_exists);
}

TEST(OmahaRequestActionTest, NetworkFailureBadHTTPCodeTest) {
  OmahaResponse response;
  ASSERT_FALSE(
      TestUpdateCheck(NULL,  // prefs
                      kDefaultTestParams,
                      "",
                      1500,
                      static_cast<ActionExitCode>(
                          kActionCodeOmahaRequestHTTPResponseBase + 999),
                      &response,
                      NULL));
  EXPECT_FALSE(response.update_exists);
}

}  // namespace chromeos_update_engine
