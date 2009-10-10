// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <glib.h>
#include <gtest/gtest.h>
#include "update_engine/action_pipe.h"
#include "update_engine/update_check_action.h"
#include "update_engine/mock_http_fetcher.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/test_utils.h"

namespace chromeos_update_engine {

using std::string;

class UpdateCheckActionTest : public ::testing::Test { };

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
      "xmlns=\"http://www.google.com/update2/response\" protocol=\"2.0\"><app "
      "appid=\"") + app_id + "\" status=\"ok\"><ping "
      "status=\"ok\"/><updatecheck DisplayVersion=\"" + display_version + "\" "
      "MoreInfo=\"" + more_info_url + "\" Prompt=\"" + prompt + "\" "
      "codebase=\"" + codebase + "\" "
      "hash=\"" + hash + "\" needsadmin=\"" + needsadmin + "\" "
      "size=\"" + size + "\" status=\"ok\"/></app></gupdate>";
}

class UpdateCheckActionTestProcessorDelegate : public ActionProcessorDelegate {
 public:
  UpdateCheckActionTestProcessorDelegate()
      : loop_(NULL),
        expected_success_(true) {}
  virtual ~UpdateCheckActionTestProcessorDelegate() {
  }
  virtual void ProcessingDone(const ActionProcessor* processor) {
    ASSERT_TRUE(loop_);
    g_main_loop_quit(loop_);
  }

  virtual void ActionCompleted(const ActionProcessor* processor,
                               const AbstractAction* action,
                               bool success) {
    // make sure actions always succeed
    if (action->Type() == "UpdateCheckAction")
      EXPECT_EQ(expected_success_, success);
    else
      EXPECT_TRUE(success);
  }
  GMainLoop *loop_;
  bool expected_success_;
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
  typedef UpdateCheckResponse InputObjectType;
  // On success, puts the output path on output
  typedef NoneType OutputObjectType;
};

class OutputObjectCollectorAction : public Action<OutputObjectCollectorAction> {
 public:
  void PerformAction() {
    // copy input object
    has_input_object_ = HasInputObject();
    if (has_input_object_)
      update_check_response_ = GetInputObject();
    processor_->ActionComplete(this, true);
  }
  // Should never be called
  void TerminateProcessing() {
    CHECK(false);
  }
  // Debugging/logging
  std::string Type() const { return "OutputObjectCollectorAction"; }
  bool has_input_object_;
  UpdateCheckResponse update_check_response_;
};

// returns true iff an output response was obtained from the
// UpdateCheckAction. out_response may be NULL.
// out_post_data may be null; if non-null, the post-data received by the
// mock HttpFetcher is returned.
bool TestUpdateCheckAction(const UpdateCheckParams& params,
                           const string& http_response,
                           bool expected_success,
                           UpdateCheckResponse* out_response,
                           vector<char> *out_post_data) {
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);
  MockHttpFetcher *fetcher = new MockHttpFetcher(http_response.data(),
                                                 http_response.size());

  UpdateCheckAction action(params, fetcher);  // takes ownership of fetcher
  UpdateCheckActionTestProcessorDelegate delegate;
  delegate.loop_ = loop;
  delegate.expected_success_ = expected_success;
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
    *out_response = collector_action.update_check_response_;
  if (out_post_data)
    *out_post_data = fetcher->post_data();
  return collector_action.has_input_object_;
}

TEST(UpdateCheckActionTest, NoUpdateTest) {
  UpdateCheckParams params("",  // machine_id
                           "",  // user_id
                           UpdateCheckParams::kOsPlatform,
                           UpdateCheckParams::kOsVersion,
                           "",  // os_sp
                           UpdateCheckParams::kAppId,
                           "0.1.0.0",
                           "en-US",
                           "unittest");
  UpdateCheckResponse response;
  ASSERT_TRUE(
      TestUpdateCheckAction(params,
                            GetNoUpdateResponse(UpdateCheckParams::kAppId),
                            true,
                            &response,
                            NULL));
  EXPECT_FALSE(response.update_exists);
}

TEST(UpdateCheckActionTest, ValidUpdateTest) {
  UpdateCheckParams params("machine_id",
                           "user_id",
                           UpdateCheckParams::kOsPlatform,
                           UpdateCheckParams::kOsVersion,
                           "service_pack",
                           UpdateCheckParams::kAppId,
                           "0.1.0.0",
                           "en-US",
                           "unittest_track");
  UpdateCheckResponse response;
  ASSERT_TRUE(
      TestUpdateCheckAction(params,
                            GetUpdateResponse(UpdateCheckParams::kAppId,
                                              "1.2.3.4",  // version
                                              "http://more/info",
                                              "true",  // prompt
                                              "http://code/base",  // dl url
                                              "HASH1234=",  // checksum
                                              "false",  // needs admin
                                              "123"),  // size
                            true,
                            &response,
                            NULL));
  EXPECT_TRUE(response.update_exists);
  EXPECT_EQ("1.2.3.4", response.display_version);
  EXPECT_EQ("http://code/base", response.codebase);
  EXPECT_EQ("http://more/info", response.more_info_url);
  EXPECT_EQ("HASH1234=", response.hash);
  EXPECT_EQ(123, response.size);
  EXPECT_FALSE(response.needs_admin);
  EXPECT_TRUE(response.prompt);
}

TEST(UpdateCheckActionTest, NoOutputPipeTest) {
  UpdateCheckParams params("",  // machine_id
                           "",  // usr_id
                           UpdateCheckParams::kOsPlatform,
                           UpdateCheckParams::kOsVersion,
                           "",  // os_sp
                           UpdateCheckParams::kAppId,
                           "0.1.0.0",
                           "en-US",
                           "unittest");
  const string http_response(GetNoUpdateResponse(UpdateCheckParams::kAppId));

  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);

  UpdateCheckAction action(params,
                           new MockHttpFetcher(http_response.data(),
                                               http_response.size()));
  UpdateCheckActionTestProcessorDelegate delegate;
  delegate.loop_ = loop;
  ActionProcessor processor;
  processor.set_delegate(&delegate);
  processor.EnqueueAction(&action);

  g_timeout_add(0, &StartProcessorInRunLoop, &processor);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
  EXPECT_FALSE(processor.IsRunning());
}

TEST(UpdateCheckActionTest, InvalidXmlTest) {
  UpdateCheckParams params("machine_id",
                           "user_id",
                           UpdateCheckParams::kOsPlatform,
                           UpdateCheckParams::kOsVersion,
                           "service_pack",
                           UpdateCheckParams::kAppId,
                           "0.1.0.0",
                           "en-US",
                           "unittest_track");
  UpdateCheckResponse response;
  ASSERT_TRUE(
      TestUpdateCheckAction(params,
                            "invalid xml>",
                            false,
                            &response,
                            NULL));
  EXPECT_FALSE(response.update_exists);
}

TEST(UpdateCheckActionTest, MissingStatusTest) {
  UpdateCheckParams params("machine_id",
                           "user_id",
                           UpdateCheckParams::kOsPlatform,
                           UpdateCheckParams::kOsVersion,
                           "service_pack",
                           UpdateCheckParams::kAppId,
                           "0.1.0.0",
                           "en-US",
                           "unittest_track");
  UpdateCheckResponse response;
  ASSERT_TRUE(TestUpdateCheckAction(
      params,
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><gupdate "
      "xmlns=\"http://www.google.com/update2/response\" protocol=\"2.0\"><app "
      "appid=\"foo\" status=\"ok\"><ping "
      "status=\"ok\"/><updatecheck/></app></gupdate>",
      false,
      &response,
      NULL));
  EXPECT_FALSE(response.update_exists);
}

TEST(UpdateCheckActionTest, InvalidStatusTest) {
  UpdateCheckParams params("machine_id",
                           "user_id",
                           UpdateCheckParams::kOsPlatform,
                           UpdateCheckParams::kOsVersion,
                           "service_pack",
                           UpdateCheckParams::kAppId,
                           "0.1.0.0",
                           "en-US",
                           "unittest_track");
  UpdateCheckResponse response;
  ASSERT_TRUE(TestUpdateCheckAction(
      params,
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><gupdate "
      "xmlns=\"http://www.google.com/update2/response\" protocol=\"2.0\"><app "
      "appid=\"foo\" status=\"ok\"><ping "
      "status=\"ok\"/><updatecheck status=\"foo\"/></app></gupdate>",
      false,
      &response,
      NULL));
  EXPECT_FALSE(response.update_exists);
}

TEST(UpdateCheckActionTest, MissingNodesetTest) {
  UpdateCheckParams params("machine_id",
                           "user_id",
                           UpdateCheckParams::kOsPlatform,
                           UpdateCheckParams::kOsVersion,
                           "service_pack",
                           UpdateCheckParams::kAppId,
                           "0.1.0.0",
                           "en-US",
                           "unittest_track");
  UpdateCheckResponse response;
  ASSERT_TRUE(TestUpdateCheckAction(
      params,
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><gupdate "
      "xmlns=\"http://www.google.com/update2/response\" protocol=\"2.0\"><app "
      "appid=\"foo\" status=\"ok\"><ping "
      "status=\"ok\"/></app></gupdate>",
      false,
      &response,
      NULL));
  EXPECT_FALSE(response.update_exists);
}

TEST(UpdateCheckActionTest, MissingFieldTest) {
  UpdateCheckParams params("machine_id",
                           "user_id",
                           UpdateCheckParams::kOsPlatform,
                           UpdateCheckParams::kOsVersion,
                           "service_pack",
                           UpdateCheckParams::kAppId,
                           "0.1.0.0",
                           "en-US",
                           "unittest_track");
  UpdateCheckResponse response;
  ASSERT_TRUE(TestUpdateCheckAction(params,
      string("<?xml version=\"1.0\" encoding=\"UTF-8\"?><gupdate "
          "xmlns=\"http://www.google.com/update2/response\" "
          "protocol=\"2.0\"><app  appid=\"") +
          UpdateCheckParams::kAppId + "\" status=\"ok\"><ping "
          "status=\"ok\"/><updatecheck DisplayVersion=\"1.2.3.4\" "
          "Prompt=\"false\" "
          "codebase=\"http://code/base\" "
          "hash=\"HASH1234=\" needsadmin=\"true\" "
          "size=\"123\" status=\"ok\"/></app></gupdate>",
      true,
      &response,
      NULL));
  EXPECT_TRUE(response.update_exists);
  EXPECT_EQ("1.2.3.4", response.display_version);
  EXPECT_EQ("http://code/base", response.codebase);
  EXPECT_EQ("", response.more_info_url);
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

TEST(UpdateCheckActionTest, TerminateTransferTest) {
  UpdateCheckParams params("",  // machine_id
                           "",  // usr_id
                           UpdateCheckParams::kOsPlatform,
                           UpdateCheckParams::kOsVersion,
                           "",  // os_sp
                           UpdateCheckParams::kAppId,
                           "0.1.0.0",
                           "en-US",
                           "unittest");
  string http_response("doesn't matter");
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);

  UpdateCheckAction action(params,
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

TEST(UpdateCheckActionTest, XmlEncodeTest) {
  EXPECT_EQ("ab", XmlEncode("ab"));
  EXPECT_EQ("a&lt;b", XmlEncode("a<b"));
  EXPECT_EQ("foo-&#x3A9;", XmlEncode("foo-\xce\xa9"));
  EXPECT_EQ("&lt;&amp;&gt;", XmlEncode("<&>"));
  EXPECT_EQ("&amp;lt;&amp;amp;&amp;gt;", XmlEncode("&lt;&amp;&gt;"));

  vector<char> post_data;

  // Make sure XML Encode is being called on the params
  UpdateCheckParams params("testthemachine<id",
                           "testtheuser_id&lt;",
                           UpdateCheckParams::kOsPlatform,
                           UpdateCheckParams::kOsVersion,
                           "testtheservice_pack>",
                           UpdateCheckParams::kAppId,
                           "0.1.0.0",
                           "en-US",
                           "unittest_track");
  UpdateCheckResponse response;
  ASSERT_TRUE(
      TestUpdateCheckAction(params,
                            "invalid xml>",
                            false,
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
}

TEST(UpdateCheckActionTest, XmlDecodeTest) {
  UpdateCheckParams params("machine_id",
                           "user_id",
                           UpdateCheckParams::kOsPlatform,
                           UpdateCheckParams::kOsVersion,
                           "service_pack",
                           UpdateCheckParams::kAppId,
                           "0.1.0.0",
                           "en-US",
                           "unittest_track");
  UpdateCheckResponse response;
  ASSERT_TRUE(
      TestUpdateCheckAction(params,
                            GetUpdateResponse(UpdateCheckParams::kAppId,
                                              "1.2.3.4",  // version
                                              "testthe&lt;url",  // more info
                                              "true",  // prompt
                                              "testthe&amp;codebase",  // dl url
                                              "HASH1234=", // checksum
                                              "false",  // needs admin
                                              "123"),  // size
                            true,
                            &response,
                            NULL));

  EXPECT_EQ(response.more_info_url, "testthe<url");
  EXPECT_EQ(response.codebase, "testthe&codebase");
}

TEST(UpdateCheckActionTest, ParseIntTest) {
  UpdateCheckParams params("machine_id",
                           "user_id",
                           UpdateCheckParams::kOsPlatform,
                           UpdateCheckParams::kOsVersion,
                           "service_pack",
                           UpdateCheckParams::kAppId,
                           "0.1.0.0",
                           "en-US",
                           "unittest_track");
  UpdateCheckResponse response;
  ASSERT_TRUE(
      TestUpdateCheckAction(params,
                            GetUpdateResponse(UpdateCheckParams::kAppId,
                                              "1.2.3.4",  // version
                                              "theurl",  // more info
                                              "true",  // prompt
                                              "thecodebase",  // dl url
                                              "HASH1234=", // checksum
                                              "false",  // needs admin
                                              // overflows int32:
                                              "123123123123123"),  // size
                            true,
                            &response,
                            NULL));

  EXPECT_EQ(response.size, 123123123123123ll);
}

}  // namespace chromeos_update_engine
