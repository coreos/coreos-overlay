// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <glib.h>

#include "base/string_util.h"
#include "gtest/gtest.h"
#include "update_engine/action_pipe.h"
#include "update_engine/mock_http_fetcher.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/omaha_request_action.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/test_utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

class OmahaRequestActionTest : public ::testing::Test { };

namespace {
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
                         const string& size) {
  return string("<?xml version=\"1.0\" encoding=\"UTF-8\"?><gupdate "
                "xmlns=\"http://www.google.com/update2/response\" "
                "protocol=\"2.0\"><app "
                "appid=\"") + app_id + "\" status=\"ok\"><ping "
      "status=\"ok\"/><updatecheck DisplayVersion=\"" + display_version + "\" "
      "MoreInfo=\"" + more_info_url + "\" Prompt=\"" + prompt + "\" "
      "IsDelta=\"true\" "
      "codebase=\"" + codebase + "\" "
      "hash=\"" + hash + "\" needsadmin=\"" + needsadmin + "\" "
      "size=\"" + size + "\" status=\"ok\"/></app></gupdate>";
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

// returns true iff an output response was obtained from the
// OmahaRequestAction. out_response may be NULL.
// out_post_data may be null; if non-null, the post-data received by the
// mock HttpFetcher is returned.
bool TestUpdateCheck(const OmahaRequestParams& params,
                     const string& http_response,
                     ActionExitCode expected_code,
                     OmahaResponse* out_response,
                     vector<char>* out_post_data) {
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  MockHttpFetcher* fetcher = new MockHttpFetcher(http_response.data(),
                                                 http_response.size());
  OmahaRequestAction action(params, NULL, fetcher);
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
  OmahaRequestAction action(params, event, fetcher);
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
  OmahaRequestParams params("",  // machine_id
                            "",  // user_id
                            OmahaRequestParams::kOsPlatform,
                            OmahaRequestParams::kOsVersion,
                            "",  // os_sp
                            "x86-generic",
                            OmahaRequestParams::kAppId,
                            "0.1.0.0",
                            "en-US",
                            "unittest",
                            false,  // delta okay
                            "");  // url
  OmahaResponse response;
  ASSERT_TRUE(
      TestUpdateCheck(params,
                      GetNoUpdateResponse(OmahaRequestParams::kAppId),
                      kActionCodeSuccess,
                      &response,
                      NULL));
  EXPECT_FALSE(response.update_exists);
}

TEST(OmahaRequestActionTest, ValidUpdateTest) {
  OmahaRequestParams params("machine_id",
                            "user_id",
                            OmahaRequestParams::kOsPlatform,
                            OmahaRequestParams::kOsVersion,
                            "service_pack",
                            "arm-generic",
                            OmahaRequestParams::kAppId,
                            "0.1.0.0",
                            "en-US",
                            "unittest_track",
                            false,  // delta okay
                            "");  // url
  OmahaResponse response;
  ASSERT_TRUE(
      TestUpdateCheck(params,
                      GetUpdateResponse(OmahaRequestParams::kAppId,
                                        "1.2.3.4",  // version
                                        "http://more/info",
                                        "true",  // prompt
                                        "http://code/base",  // dl url
                                        "HASH1234=",  // checksum
                                        "false",  // needs admin
                                        "123"),  // size
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
}

TEST(OmahaRequestActionTest, NoOutputPipeTest) {
  OmahaRequestParams params("",  // machine_id
                            "",  // usr_id
                            OmahaRequestParams::kOsPlatform,
                            OmahaRequestParams::kOsVersion,
                            "",  // os_sp
                            "",  // os_board
                            OmahaRequestParams::kAppId,
                            "0.1.0.0",
                            "en-US",
                            "unittest",
                            false,  // delta okay
                            "");  // url
  const string http_response(GetNoUpdateResponse(OmahaRequestParams::kAppId));

  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);

  OmahaRequestAction action(params, NULL,
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
  OmahaRequestParams params("machine_id",
                            "user_id",
                            OmahaRequestParams::kOsPlatform,
                            OmahaRequestParams::kOsVersion,
                            "service_pack",
                            "x86-generic",
                            OmahaRequestParams::kAppId,
                            "0.1.0.0",
                            "en-US",
                            "unittest_track",
                            false,  // delta okay
                            "http://url");
  OmahaResponse response;
  ASSERT_FALSE(
      TestUpdateCheck(params,
                      "invalid xml>",
                      kActionCodeError,
                      &response,
                      NULL));
  EXPECT_FALSE(response.update_exists);
}

TEST(OmahaRequestActionTest, MissingStatusTest) {
  OmahaRequestParams params("machine_id",
                            "user_id",
                            OmahaRequestParams::kOsPlatform,
                            OmahaRequestParams::kOsVersion,
                            "service_pack",
                            "x86-generic",
                            OmahaRequestParams::kAppId,
                            "0.1.0.0",
                            "en-US",
                            "unittest_track",
                            false,  // delta okay
                            "http://url");
  OmahaResponse response;
  ASSERT_FALSE(TestUpdateCheck(
      params,
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><gupdate "
      "xmlns=\"http://www.google.com/update2/response\" protocol=\"2.0\"><app "
      "appid=\"foo\" status=\"ok\"><ping "
      "status=\"ok\"/><updatecheck/></app></gupdate>",
      kActionCodeError,
      &response,
      NULL));
  EXPECT_FALSE(response.update_exists);
}

TEST(OmahaRequestActionTest, InvalidStatusTest) {
  OmahaRequestParams params("machine_id",
                            "user_id",
                            OmahaRequestParams::kOsPlatform,
                            OmahaRequestParams::kOsVersion,
                            "service_pack",
                            "x86-generic",
                            OmahaRequestParams::kAppId,
                            "0.1.0.0",
                            "en-US",
                            "unittest_track",
                            false,  // delta okay
                            "http://url");
  OmahaResponse response;
  ASSERT_FALSE(TestUpdateCheck(
      params,
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><gupdate "
      "xmlns=\"http://www.google.com/update2/response\" protocol=\"2.0\"><app "
      "appid=\"foo\" status=\"ok\"><ping "
      "status=\"ok\"/><updatecheck status=\"foo\"/></app></gupdate>",
      kActionCodeError,
      &response,
      NULL));
  EXPECT_FALSE(response.update_exists);
}

TEST(OmahaRequestActionTest, MissingNodesetTest) {
  OmahaRequestParams params("machine_id",
                            "user_id",
                            OmahaRequestParams::kOsPlatform,
                            OmahaRequestParams::kOsVersion,
                            "service_pack",
                            "x86-generic",
                            OmahaRequestParams::kAppId,
                            "0.1.0.0",
                            "en-US",
                            "unittest_track",
                            false,  // delta okay
                            "http://url");
  OmahaResponse response;
  ASSERT_FALSE(TestUpdateCheck(
      params,
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><gupdate "
      "xmlns=\"http://www.google.com/update2/response\" protocol=\"2.0\"><app "
      "appid=\"foo\" status=\"ok\"><ping "
      "status=\"ok\"/></app></gupdate>",
      kActionCodeError,
      &response,
      NULL));
  EXPECT_FALSE(response.update_exists);
}

TEST(OmahaRequestActionTest, MissingFieldTest) {
  OmahaRequestParams params("machine_id",
                            "user_id",
                            OmahaRequestParams::kOsPlatform,
                            OmahaRequestParams::kOsVersion,
                            "service_pack",
                            "x86-generic",
                            OmahaRequestParams::kAppId,
                            "0.1.0.0",
                            "en-US",
                            "unittest_track",
                            false,  // delta okay
                            "http://url");
  OmahaResponse response;
  ASSERT_TRUE(TestUpdateCheck(params,
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
                              "codebase=\"http://code/base\" "
                              "hash=\"HASH1234=\" needsadmin=\"true\" "
                              "size=\"123\" "
                              "status=\"ok\"/></app></gupdate>",
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
  OmahaRequestParams params("",  // machine_id
                            "",  // usr_id
                            OmahaRequestParams::kOsPlatform,
                            OmahaRequestParams::kOsVersion,
                            "",  // os_sp
                            "",  // os_board
                            OmahaRequestParams::kAppId,
                            "0.1.0.0",
                            "en-US",
                            "unittest",
                            false,  // delta okay
                            "http://url");
  string http_response("doesn't matter");
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);

  OmahaRequestAction action(params, NULL,
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
  OmahaRequestParams params("testthemachine<id",
                            "testtheuser_id&lt;",
                            OmahaRequestParams::kOsPlatform,
                            OmahaRequestParams::kOsVersion,
                            "testtheservice_pack>",
                            "x86 generic",
                            OmahaRequestParams::kAppId,
                            "0.1.0.0",
                            "en-US",
                            "unittest_track",
                            false,  // delta okay
                            "http://url");
  OmahaResponse response;
  ASSERT_FALSE(
      TestUpdateCheck(params,
                      "invalid xml>",
                      kActionCodeError,
                      &response,
                      &post_data));
  // convert post_data to string
  string post_str(&post_data[0], post_data.size());
  EXPECT_NE(post_str.find("testthemachine&lt;id"), string::npos);
  EXPECT_EQ(post_str.find("testthemachine<id"), string::npos);
  EXPECT_NE(post_str.find("testtheuser_id&amp;lt;"), string::npos);
  EXPECT_EQ(post_str.find("testtheuser_id&lt;"), string::npos);
  EXPECT_NE(post_str.find("testtheservice_pack&gt;"), string::npos);
  EXPECT_EQ(post_str.find("testtheservice_pack>"), string::npos);
  EXPECT_NE(post_str.find("x86 generic"), string::npos);
}

TEST(OmahaRequestActionTest, XmlDecodeTest) {
  OmahaRequestParams params("machine_id",
                            "user_id",
                            OmahaRequestParams::kOsPlatform,
                            OmahaRequestParams::kOsVersion,
                            "service_pack",
                            "x86-generic",
                            OmahaRequestParams::kAppId,
                            "0.1.0.0",
                            "en-US",
                            "unittest_track",
                            false,  // delta okay
                            "http://url");
  OmahaResponse response;
  ASSERT_TRUE(
      TestUpdateCheck(params,
                      GetUpdateResponse(OmahaRequestParams::kAppId,
                                        "1.2.3.4",  // version
                                        "testthe&lt;url",  // more info
                                        "true",  // prompt
                                        "testthe&amp;codebase",  // dl url
                                        "HASH1234=", // checksum
                                        "false",  // needs admin
                                        "123"),  // size
                      kActionCodeSuccess,
                      &response,
                      NULL));

  EXPECT_EQ(response.more_info_url, "testthe<url");
  EXPECT_EQ(response.codebase, "testthe&codebase");
}

TEST(OmahaRequestActionTest, ParseIntTest) {
  OmahaRequestParams params("machine_id",
                            "user_id",
                            OmahaRequestParams::kOsPlatform,
                            OmahaRequestParams::kOsVersion,
                            "service_pack",
                            "the_board",
                            OmahaRequestParams::kAppId,
                            "0.1.0.0",
                            "en-US",
                            "unittest_track",
                            false,  // delta okay
                            "http://url");
  OmahaResponse response;
  ASSERT_TRUE(
      TestUpdateCheck(params,
                      GetUpdateResponse(OmahaRequestParams::kAppId,
                                        "1.2.3.4",  // version
                                        "theurl",  // more info
                                        "true",  // prompt
                                        "thecodebase",  // dl url
                                        "HASH1234=", // checksum
                                        "false",  // needs admin
                                        // overflows int32:
                                        "123123123123123"),  // size
                      kActionCodeSuccess,
                      &response,
                      NULL));

  EXPECT_EQ(response.size, 123123123123123ll);
}

TEST(OmahaRequestActionTest, FormatUpdateCheckOutputTest) {
  vector<char> post_data;
  OmahaRequestParams params("machine_id",
                            "user_id",
                            OmahaRequestParams::kOsPlatform,
                            OmahaRequestParams::kOsVersion,
                            "service_pack",
                            "x86-generic",
                            OmahaRequestParams::kAppId,
                            "0.1.0.0",
                            "en-US",
                            "unittest_track",
                            false,  // delta okay
                            "http://url");
  OmahaResponse response;
  ASSERT_FALSE(TestUpdateCheck(params,
                               "invalid xml>",
                               kActionCodeError,
                               &response,
                               &post_data));
  // convert post_data to string
  string post_str(&post_data[0], post_data.size());
  EXPECT_NE(post_str.find("        <o:ping active=\"0\"></o:ping>\n"
                          "        <o:updatecheck></o:updatecheck>\n"),
            string::npos);
  EXPECT_EQ(post_str.find("o:event"), string::npos);
}

TEST(OmahaRequestActionTest, FormatEventOutputTest) {
  vector<char> post_data;
  OmahaRequestParams params("machine_id",
                            "user_id",
                            OmahaRequestParams::kOsPlatform,
                            OmahaRequestParams::kOsVersion,
                            "service_pack",
                            "x86-generic",
                            OmahaRequestParams::kAppId,
                            "0.1.0.0",
                            "en-US",
                            "unittest_track",
                            false,  // delta okay
                            "http://url");
  TestEvent(params,
            new OmahaEvent(OmahaEvent::kTypeDownloadComplete,
                           OmahaEvent::kResultError,
                           5),
            "invalid xml>",
            &post_data);
  // convert post_data to string
  string post_str(&post_data[0], post_data.size());
  string expected_event = StringPrintf(
      "        <o:event eventtype=\"%d\" eventresult=\"%d\" "
      "errorcode=\"%d\"></o:event>\n",
      OmahaEvent::kTypeDownloadComplete,
      OmahaEvent::kResultError,
      5);
  EXPECT_NE(post_str.find(expected_event), string::npos);
  EXPECT_EQ(post_str.find("o:updatecheck"), string::npos);
}

TEST(OmahaRequestActionTest, IsEventTest) {
  string http_response("doesn't matter");
  OmahaRequestParams params("machine_id",
                            "user_id",
                            OmahaRequestParams::kOsPlatform,
                            OmahaRequestParams::kOsVersion,
                            "service_pack",
                            "x86-generic",
                            OmahaRequestParams::kAppId,
                            "0.1.0.0",
                            "en-US",
                            "unittest_track",
                            false,  // delta okay
                            "http://url");

  OmahaRequestAction update_check_action(
      params,
      NULL,
      new MockHttpFetcher(http_response.data(),
                          http_response.size()));
  EXPECT_FALSE(update_check_action.IsEvent());

  OmahaRequestAction event_action(
      params,
      new OmahaEvent(OmahaEvent::kTypeInstallComplete,
                     OmahaEvent::kResultError,
                     0),
      new MockHttpFetcher(http_response.data(),
                          http_response.size()));
  EXPECT_TRUE(event_action.IsEvent());
}

TEST(OmahaRequestActionTest, FormatDeltaOkayOutputTest) {
  for (int i = 0; i < 2; i++) {
    bool delta_okay = i == 1;
    const char* delta_okay_str = delta_okay ? "true" : "false";
    vector<char> post_data;
    OmahaRequestParams params("machine_id",
                              "user_id",
                              OmahaRequestParams::kOsPlatform,
                              OmahaRequestParams::kOsVersion,
                              "service_pack",
                              "x86-generic",
                              OmahaRequestParams::kAppId,
                              "0.1.0.0",
                              "en-US",
                              "unittest_track",
                              delta_okay,
                              "http://url");
    ASSERT_FALSE(TestUpdateCheck(params,
                                 "invalid xml>",
                                 kActionCodeError,
                                 NULL,
                                 &post_data));
    // convert post_data to string
    string post_str(&post_data[0], post_data.size());
    EXPECT_NE(post_str.find(StringPrintf(" delta_okay=\"%s\"", delta_okay_str)),
              string::npos)
        << "i = " << i;
  }
}

}  // namespace chromeos_update_engine
