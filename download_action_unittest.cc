// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>
#include <glib.h>
#include <gtest/gtest.h>
#include "update_engine/action_pipe.h"
#include "update_engine/download_action.h"
#include "update_engine/mock_http_fetcher.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

namespace chromeos_update_engine {

using std::string;
using std::vector;

class DownloadActionTest : public ::testing::Test { };

namespace {
class DownloadActionTestProcessorDelegate : public ActionProcessorDelegate {
 public:
  DownloadActionTestProcessorDelegate()
      : loop_(NULL), processing_done_called_(false) {}
  virtual ~DownloadActionTestProcessorDelegate() {
    EXPECT_TRUE(processing_done_called_);
  }
  virtual void ProcessingDone(const ActionProcessor* processor, bool success) {
    ASSERT_TRUE(loop_);
    g_main_loop_quit(loop_);
    vector<char> found_data;
    ASSERT_TRUE(utils::ReadFile(path_, &found_data));
    ASSERT_EQ(expected_data_.size(), found_data.size());
    for (unsigned i = 0; i < expected_data_.size(); i++) {
      EXPECT_EQ(expected_data_[i], found_data[i]);
    }
    processing_done_called_ = true;
  }

  virtual void ActionCompleted(ActionProcessor* processor,
                               AbstractAction* action,
                               bool success) {
    // make sure actions always succeed
    EXPECT_TRUE(success);
  }

  GMainLoop *loop_;
  string path_;
  vector<char> expected_data_;
  bool processing_done_called_;
};

struct EntryPointArgs {
  const vector<char> *data;
  GMainLoop *loop;
  ActionProcessor *processor;
};

gboolean StartProcessorInRunLoop(gpointer data) {
  ActionProcessor *processor = reinterpret_cast<ActionProcessor*>(data);
  processor->StartProcessing();
  return FALSE;
}

void TestWithData(const vector<char>& data, bool compress) {
  vector<char> use_data;
  if (compress) {
    use_data = GzipCompressData(data);
  } else {
    use_data = data;
  }

  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);

  // TODO(adlr): see if we need a different file for build bots
  const string path("/tmp/DownloadActionTest");
  // takes ownership of passed in HttpFetcher
  InstallPlan install_plan(compress, "",
                           OmahaHashCalculator::OmahaHashOfData(use_data),
                           path);
  ObjectFeederAction<InstallPlan> feeder_action;
  feeder_action.set_obj(install_plan);
  DownloadAction download_action(new MockHttpFetcher(&use_data[0],
                                                     use_data.size()));
  BondActions(&feeder_action, &download_action);

  DownloadActionTestProcessorDelegate delegate;
  delegate.loop_ = loop;
  delegate.expected_data_ = data;
  delegate.path_ = path;
  ActionProcessor processor;
  processor.set_delegate(&delegate);
  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&download_action);

  g_timeout_add(0, &StartProcessorInRunLoop, &processor);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);

  // remove temp file; don't care if there are errors here
  unlink(path.c_str());
}
}  // namespace {}

TEST(DownloadActionTest, SimpleTest) {
  vector<char> small;
  const char* foo = "foo";
  small.insert(small.end(), foo, foo + strlen(foo));
  TestWithData(small, false);
  TestWithData(small, true);
}

TEST(DownloadActionTest, LargeTest) {
  vector<char> big(5 * kMockHttpFetcherChunkSize);
  char c = '0';
  for (unsigned int i = 0; i < big.size(); i++) {
    big[i] = c;
    if ('9' == c)
      c = '0';
    else
      c++;
  }
  TestWithData(big, false);
  TestWithData(big, true);
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

gboolean TerminateEarlyTestStarter(gpointer data) {
  ActionProcessor *processor = reinterpret_cast<ActionProcessor*>(data);
  processor->StartProcessing();
  CHECK(processor->IsRunning());
  processor->StopProcessing();
  return FALSE;
}

}  // namespace {}

TEST(DownloadActionTest, TerminateEarlyTest) {
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);

  vector<char> data(kMockHttpFetcherChunkSize + kMockHttpFetcherChunkSize / 2);
  memset(&data[0], 0, data.size());

  const string path("/tmp/DownloadActionTest");
  {
    // takes ownership of passed in HttpFetcher
    ObjectFeederAction<InstallPlan> feeder_action;
    InstallPlan install_plan(false, "", "", path);
    feeder_action.set_obj(install_plan);
    DownloadAction download_action(new MockHttpFetcher(&data[0], data.size()));
    TerminateEarlyTestProcessorDelegate delegate;
    delegate.loop_ = loop;
    ActionProcessor processor;
    processor.set_delegate(&delegate);
    processor.EnqueueAction(&feeder_action);
    processor.EnqueueAction(&download_action);
    BondActions(&feeder_action, &download_action);

    g_timeout_add(0, &TerminateEarlyTestStarter, &processor);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
  }

  // 1 or 0 chunks should have come through
  const off_t resulting_file_size(FileSize(path));
  if (resulting_file_size != 0)
    EXPECT_EQ(kMockHttpFetcherChunkSize, resulting_file_size);
}

class DownloadActionTestAction;

template<>
class ActionTraits<DownloadActionTestAction> {
 public:
  typedef InstallPlan OutputObjectType;
  typedef InstallPlan InputObjectType;
};

// This is a simple Action class for testing.
struct DownloadActionTestAction : public Action<DownloadActionTestAction> {
  DownloadActionTestAction() : did_run_(false) {}
  typedef InstallPlan InputObjectType;
  typedef InstallPlan OutputObjectType;
  ActionPipe<InstallPlan>* in_pipe() { return in_pipe_.get(); }
  ActionPipe<InstallPlan>* out_pipe() { return out_pipe_.get(); }
  ActionProcessor* processor() { return processor_; }
  void PerformAction() {
    did_run_ = true;
    ASSERT_TRUE(HasInputObject());
    EXPECT_TRUE(expected_input_object_ == GetInputObject());
    ASSERT_TRUE(processor());
    processor()->ActionComplete(this, true);
  }
  string Type() const { return "DownloadActionTestAction"; }
  InstallPlan expected_input_object_;
  bool did_run_;
};

namespace {
// This class is an ActionProcessorDelegate that simply terminates the
// run loop when the ActionProcessor has completed processing. It's used
// only by the test PassObjectOutTest.
class PassObjectOutTestProcessorDelegate : public ActionProcessorDelegate {
 public:
  void ProcessingDone(const ActionProcessor* processor, bool success) {
    ASSERT_TRUE(loop_);
    g_main_loop_quit(loop_);
  }
  GMainLoop *loop_;
};

gboolean PassObjectOutTestStarter(gpointer data) {
  ActionProcessor *processor = reinterpret_cast<ActionProcessor*>(data);
  processor->StartProcessing();
  return FALSE;
}
}

TEST(DownloadActionTest, PassObjectOutTest) {
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);

  // takes ownership of passed in HttpFetcher
  InstallPlan install_plan(false, "",
                           OmahaHashCalculator::OmahaHashOfString("x"),
                           "/dev/null");
  ObjectFeederAction<InstallPlan> feeder_action;
  feeder_action.set_obj(install_plan);
  DownloadAction download_action(new MockHttpFetcher("x", 1));

  DownloadActionTestAction test_action;
  test_action.expected_input_object_ = install_plan;
  BondActions(&feeder_action, &download_action);
  BondActions(&download_action, &test_action);

  ActionProcessor processor;
  PassObjectOutTestProcessorDelegate delegate;
  delegate.loop_ = loop;
  processor.set_delegate(&delegate);
  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&download_action);
  processor.EnqueueAction(&test_action);

  g_timeout_add(0, &PassObjectOutTestStarter, &processor);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);

  EXPECT_EQ(true, test_action.did_run_);
}

TEST(DownloadActionTest, BadOutFileTest) {
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);

  const string path("/fake/path/that/cant/be/created/because/of/missing/dirs");

  // takes ownership of passed in HttpFetcher
  InstallPlan install_plan(false, "", "", path);
  ObjectFeederAction<InstallPlan> feeder_action;
  feeder_action.set_obj(install_plan);
  DownloadAction download_action(new MockHttpFetcher("x", 1));
  BondActions(&feeder_action, &download_action);

  ActionProcessor processor;
  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&download_action);
  processor.StartProcessing();
  ASSERT_FALSE(processor.IsRunning());

  g_main_loop_unref(loop);
}

}  // namespace chromeos_update_engine
