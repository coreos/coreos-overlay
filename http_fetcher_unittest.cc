// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "glib.h"
#include "gtest/gtest.h"
#include "update_engine/libcurl_http_fetcher.h"
#include "update_engine/mock_http_fetcher.h"
#include "update_engine/multi_http_fetcher.h"

using std::make_pair;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
// WARNING, if you update these, you must also update test_http_server.cc.
const char* const kServerPort = "8088";
const int kBigSize = 100000;
string LocalServerUrlForPath(const string& path) {
  return string("http://127.0.0.1:") + kServerPort + path;
}
}

template <typename T>
class HttpFetcherTest : public ::testing::Test {
 public:
  HttpFetcher* NewLargeFetcher() = 0;
  HttpFetcher* NewSmallFetcher() = 0;
  string BigUrl() const = 0;
  string SmallUrl() const = 0;
  bool IsMock() const = 0;
  bool IsMulti() const = 0;
};

class NullHttpServer {
 public:
  NullHttpServer() : started_(true) {}
  ~NullHttpServer() {}
  bool started_;
};


template <>
class HttpFetcherTest<MockHttpFetcher> : public ::testing::Test {
 public:
  HttpFetcher* NewLargeFetcher() {
    vector<char> big_data(1000000);
    return new MockHttpFetcher(big_data.data(), big_data.size());
  }
  HttpFetcher* NewSmallFetcher() {
    return new MockHttpFetcher("x", 1);
  }
  string BigUrl() const {
    return "unused://unused";
  }
  string SmallUrl() const {
    return "unused://unused";
  }
  bool IsMock() const { return true; }
  bool IsMulti() const { return false; }
  typedef NullHttpServer HttpServer;
  void IgnoreServerAborting(HttpServer* server) const {}
};

class PythonHttpServer {
 public:
  PythonHttpServer() {
    char *argv[2] = {strdup("./test_http_server"), NULL};
    GError *err;
    started_ = false;
    validate_quit_ = true;
    if (!g_spawn_async(NULL,
                       argv,
                       NULL,
                       G_SPAWN_DO_NOT_REAP_CHILD,
                       NULL,
                       NULL,
                       &pid_,
                       &err)) {
      return;
    }
    int rc = 1;
    int tries = 10;
    started_ = true;
    while (0 != rc) {
      LOG(INFO) << "running wget to start";
      rc = system((string("wget --output-document=/dev/null ") +
                   LocalServerUrlForPath("/test")).c_str());
      LOG(INFO) << "done running wget to start";
      usleep(10 * 1000);  // 10 ms
      tries--;
      if (tries == 0) {
        LOG(ERROR) << "Unable to start server.";
        started_ = false;
        break;
      }
    }
    free(argv[0]);
    LOG(INFO) << "gdb attach now!";
    return;
  }
  ~PythonHttpServer() {
    if (!started_)
      return;
    // request that the server exit itself
    LOG(INFO) << "running wget to exit";
    int rc = system((string("wget -t 1 --output-document=/dev/null ") +
                     LocalServerUrlForPath("/quitquitquit")).c_str());
    LOG(INFO) << "done running wget to exit";
    if (validate_quit_)
      EXPECT_EQ(0, rc);
    waitpid(pid_, NULL, 0);
  }
  GPid pid_;
  bool started_;
  bool validate_quit_;
};

template <>
class HttpFetcherTest<LibcurlHttpFetcher> : public ::testing::Test {
 public:
  virtual HttpFetcher* NewLargeFetcher() {
    LibcurlHttpFetcher *ret = new LibcurlHttpFetcher;
    // Speed up test execution.
    ret->set_idle_seconds(1);
    ret->set_retry_seconds(1);
    ret->SetConnectionAsExpensive(false);
    ret->SetBuildType(false);
    return ret;
  }
  HttpFetcher* NewSmallFetcher() {
    return NewLargeFetcher();
  }
  string BigUrl() const {
    return LocalServerUrlForPath("/big");
  }
  string SmallUrl() const {
    return LocalServerUrlForPath("/foo");
  }
  bool IsMock() const { return false; }
  bool IsMulti() const { return false; }
  typedef PythonHttpServer HttpServer;
  void IgnoreServerAborting(HttpServer* server) const {
    PythonHttpServer *pyserver = reinterpret_cast<PythonHttpServer*>(server);
    pyserver->validate_quit_ = false;
  }
};

template <>
class HttpFetcherTest<MultiHttpFetcher<LibcurlHttpFetcher> >
    : public HttpFetcherTest<LibcurlHttpFetcher> {
 public:
  HttpFetcher* NewLargeFetcher() {
    MultiHttpFetcher<LibcurlHttpFetcher> *ret =
        new MultiHttpFetcher<LibcurlHttpFetcher>;
    MultiHttpFetcher<LibcurlHttpFetcher>::RangesVect
        ranges(1, make_pair(0, -1));
    ret->set_ranges(ranges);
    // Speed up test execution.
    ret->set_idle_seconds(1);
    ret->set_retry_seconds(1);
    ret->SetConnectionAsExpensive(false);
    ret->SetBuildType(false);
    return ret;
  }
  bool IsMulti() const { return true; }
};

typedef ::testing::Types<LibcurlHttpFetcher,
                         MockHttpFetcher,
                         MultiHttpFetcher<LibcurlHttpFetcher> >
HttpFetcherTestTypes;
TYPED_TEST_CASE(HttpFetcherTest, HttpFetcherTestTypes);

namespace {
class HttpFetcherTestDelegate : public HttpFetcherDelegate {
 public:
  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* bytes, int length) {
    char str[length + 1];
    memset(str, 0, length + 1);
    memcpy(str, bytes, length);
  }
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) {
    EXPECT_EQ(200, fetcher->http_response_code());
    g_main_loop_quit(loop_);
  }
  virtual void TransferTerminated(HttpFetcher* fetcher) {
    ADD_FAILURE();
  }
  GMainLoop* loop_;
};

struct StartTransferArgs {
  HttpFetcher *http_fetcher;
  string url;
};

gboolean StartTransfer(gpointer data) {
  StartTransferArgs *args = reinterpret_cast<StartTransferArgs*>(data);
  args->http_fetcher->BeginTransfer(args->url);
  return FALSE;
}
}  // namespace {}

TYPED_TEST(HttpFetcherTest, SimpleTest) {
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    HttpFetcherTestDelegate delegate;
    delegate.loop_ = loop;
    scoped_ptr<HttpFetcher> fetcher(this->NewSmallFetcher());
    fetcher->set_delegate(&delegate);

    typename TestFixture::HttpServer server;
    ASSERT_TRUE(server.started_);

    StartTransferArgs start_xfer_args = {fetcher.get(), this->SmallUrl()};

    g_timeout_add(0, StartTransfer, &start_xfer_args);
    g_main_loop_run(loop);
  }
  g_main_loop_unref(loop);
}

TYPED_TEST(HttpFetcherTest, SimpleBigTest) {
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    HttpFetcherTestDelegate delegate;
    delegate.loop_ = loop;
    scoped_ptr<HttpFetcher> fetcher(this->NewLargeFetcher());
    fetcher->set_delegate(&delegate);

    typename TestFixture::HttpServer server;
    ASSERT_TRUE(server.started_);

    StartTransferArgs start_xfer_args = {fetcher.get(), this->BigUrl()};

    g_timeout_add(0, StartTransfer, &start_xfer_args);
    g_main_loop_run(loop);
  }
  g_main_loop_unref(loop);
}

namespace {
class PausingHttpFetcherTestDelegate : public HttpFetcherDelegate {
 public:
  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* bytes, int length) {
    char str[length + 1];
    memset(str, 0, length + 1);
    memcpy(str, bytes, length);
    CHECK(!paused_);
    paused_ = true;
    fetcher->Pause();
  }
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) {
    g_main_loop_quit(loop_);
  }
  virtual void TransferTerminated(HttpFetcher* fetcher) {
    ADD_FAILURE();
  }
  void Unpause() {
    CHECK(paused_);
    paused_ = false;
    fetcher_->Unpause();
  }
  bool paused_;
  HttpFetcher* fetcher_;
  GMainLoop* loop_;
};

gboolean UnpausingTimeoutCallback(gpointer data) {
  PausingHttpFetcherTestDelegate *delegate =
      reinterpret_cast<PausingHttpFetcherTestDelegate*>(data);
  if (delegate->paused_)
    delegate->Unpause();
  return TRUE;
}
}  // namespace {}

TYPED_TEST(HttpFetcherTest, PauseTest) {
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    PausingHttpFetcherTestDelegate delegate;
    scoped_ptr<HttpFetcher> fetcher(this->NewLargeFetcher());
    delegate.paused_ = false;
    delegate.loop_ = loop;
    delegate.fetcher_ = fetcher.get();
    fetcher->set_delegate(&delegate);

    typename TestFixture::HttpServer server;
    ASSERT_TRUE(server.started_);
    GSource* timeout_source_;
    timeout_source_ = g_timeout_source_new(0);  // ms
    g_source_set_callback(timeout_source_, UnpausingTimeoutCallback, &delegate,
                          NULL);
    g_source_attach(timeout_source_, NULL);
    fetcher->BeginTransfer(this->BigUrl());

    g_main_loop_run(loop);
    g_source_destroy(timeout_source_);
  }
  g_main_loop_unref(loop);
}

namespace {
class AbortingHttpFetcherTestDelegate : public HttpFetcherDelegate {
 public:
  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* bytes, int length) {}
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) {
    ADD_FAILURE();  // We should never get here
    g_main_loop_quit(loop_);
  }
  virtual void TransferTerminated(HttpFetcher* fetcher) {
    EXPECT_EQ(fetcher, fetcher_.get());
    EXPECT_FALSE(once_);
    EXPECT_TRUE(callback_once_);
    callback_once_ = false;
    // |fetcher| can be destroyed during this callback.
    fetcher_.reset(NULL);
 }
  void TerminateTransfer() {
    CHECK(once_);
    once_ = false;
    fetcher_->TerminateTransfer();
  }
  void EndLoop() {
    g_main_loop_quit(loop_);
  }
  bool once_;
  bool callback_once_;
  scoped_ptr<HttpFetcher> fetcher_;
  GMainLoop* loop_;
};

gboolean AbortingTimeoutCallback(gpointer data) {
  AbortingHttpFetcherTestDelegate *delegate =
      reinterpret_cast<AbortingHttpFetcherTestDelegate*>(data);
  if (delegate->once_) {
    delegate->TerminateTransfer();
    return TRUE;
  } else {
    delegate->EndLoop();
    return FALSE;
  }
}
}  // namespace {}

TYPED_TEST(HttpFetcherTest, AbortTest) {
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    AbortingHttpFetcherTestDelegate delegate;
    delegate.fetcher_.reset(this->NewLargeFetcher());
    delegate.once_ = true;
    delegate.callback_once_ = true;
    delegate.loop_ = loop;
    delegate.fetcher_->set_delegate(&delegate);

    typename TestFixture::HttpServer server;
    this->IgnoreServerAborting(&server);
    ASSERT_TRUE(server.started_);
    GSource* timeout_source_;
    timeout_source_ = g_timeout_source_new(0);  // ms
    g_source_set_callback(timeout_source_, AbortingTimeoutCallback, &delegate,
                          NULL);
    g_source_attach(timeout_source_, NULL);
    delegate.fetcher_->BeginTransfer(this->BigUrl());

    g_main_loop_run(loop);
    CHECK(!delegate.once_);
    CHECK(!delegate.callback_once_);
    g_source_destroy(timeout_source_);
  }
  g_main_loop_unref(loop);
}

namespace {
class FlakyHttpFetcherTestDelegate : public HttpFetcherDelegate {
 public:
  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* bytes, int length) {
    data.append(bytes, length);
  }
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) {
    EXPECT_TRUE(successful);
    EXPECT_EQ(206, fetcher->http_response_code());
    g_main_loop_quit(loop_);
  }
  virtual void TransferTerminated(HttpFetcher* fetcher) {
    ADD_FAILURE();
  }
  string data;
  GMainLoop* loop_;
};
}  // namespace {}

TYPED_TEST(HttpFetcherTest, FlakyTest) {
  if (this->IsMock())
    return;
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    FlakyHttpFetcherTestDelegate delegate;
    delegate.loop_ = loop;
    scoped_ptr<HttpFetcher> fetcher(this->NewSmallFetcher());
    fetcher->set_delegate(&delegate);

    typename TestFixture::HttpServer server;
    ASSERT_TRUE(server.started_);

    StartTransferArgs start_xfer_args = {
      fetcher.get(),
      LocalServerUrlForPath("/flaky")
    };

    g_timeout_add(0, StartTransfer, &start_xfer_args);
    g_main_loop_run(loop);

    // verify the data we get back
    ASSERT_EQ(100000, delegate.data.size());
    for (int i = 0; i < 100000; i += 10) {
      // Assert so that we don't flood the screen w/ EXPECT errors on failure.
      ASSERT_EQ(delegate.data.substr(i, 10), "abcdefghij");
    }
  }
  g_main_loop_unref(loop);
}

namespace {
class FailureHttpFetcherTestDelegate : public HttpFetcherDelegate {
 public:
  FailureHttpFetcherTestDelegate() : loop_(NULL), server_(NULL) {}
  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* bytes, int length) {
    if (server_) {
      LOG(INFO) << "Stopping server";
      delete server_;
      LOG(INFO) << "server stopped";
      server_ = NULL;
    }
  }
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) {
    EXPECT_FALSE(successful);
    EXPECT_EQ(0, fetcher->http_response_code());
    g_main_loop_quit(loop_);
  }
  virtual void TransferTerminated(HttpFetcher* fetcher) {
    ADD_FAILURE();
  }
  GMainLoop* loop_;
  PythonHttpServer* server_;
};
}  // namespace {}


TYPED_TEST(HttpFetcherTest, FailureTest) {
  if (this->IsMock())
    return;
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    FailureHttpFetcherTestDelegate delegate;
    delegate.loop_ = loop;
    scoped_ptr<HttpFetcher> fetcher(this->NewSmallFetcher());
    fetcher->set_delegate(&delegate);

    StartTransferArgs start_xfer_args = {
      fetcher.get(),
      LocalServerUrlForPath(this->SmallUrl())
    };

    g_timeout_add(0, StartTransfer, &start_xfer_args);
    g_main_loop_run(loop);

    // Exiting and testing happens in the delegate
  }
  g_main_loop_unref(loop);
}

TYPED_TEST(HttpFetcherTest, ServerDiesTest) {
  if (this->IsMock())
    return;
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    FailureHttpFetcherTestDelegate delegate;
    delegate.loop_ = loop;
    delegate.server_ = new PythonHttpServer;
    scoped_ptr<HttpFetcher> fetcher(this->NewSmallFetcher());
    fetcher->set_delegate(&delegate);

    StartTransferArgs start_xfer_args = {
      fetcher.get(),
      LocalServerUrlForPath("/flaky")
    };

    g_timeout_add(0, StartTransfer, &start_xfer_args);
    g_main_loop_run(loop);

    // Exiting and testing happens in the delegate
  }
  g_main_loop_unref(loop);
}

namespace {
const int kRedirectCodes[] = { 301, 302, 303, 307 };

class RedirectHttpFetcherTestDelegate : public HttpFetcherDelegate {
 public:
  RedirectHttpFetcherTestDelegate(bool expected_successful)
      : expected_successful_(expected_successful) {}
  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* bytes, int length) {
    data.append(bytes, length);
  }
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) {
    EXPECT_EQ(expected_successful_, successful);
    if (expected_successful_)
      EXPECT_EQ(200, fetcher->http_response_code());
    else {
      EXPECT_GE(fetcher->http_response_code(), 301);
      EXPECT_LE(fetcher->http_response_code(), 307);
    }
    g_main_loop_quit(loop_);
  }
  virtual void TransferTerminated(HttpFetcher* fetcher) {
    ADD_FAILURE();
  }
  bool expected_successful_;
  string data;
  GMainLoop* loop_;
};

// RedirectTest takes ownership of |http_fetcher|.
void RedirectTest(bool expected_successful,
                  const string& url,
                  HttpFetcher* http_fetcher) {
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  RedirectHttpFetcherTestDelegate delegate(expected_successful);
  delegate.loop_ = loop;
  scoped_ptr<HttpFetcher> fetcher(http_fetcher);
  fetcher->set_delegate(&delegate);

  StartTransferArgs start_xfer_args =
      { fetcher.get(), LocalServerUrlForPath(url) };

  g_timeout_add(0, StartTransfer, &start_xfer_args);
  g_main_loop_run(loop);
  if (expected_successful) {
    // verify the data we get back
    ASSERT_EQ(1000, delegate.data.size());
    for (int i = 0; i < 1000; i += 10) {
      // Assert so that we don't flood the screen w/ EXPECT errors on failure.
      ASSERT_EQ(delegate.data.substr(i, 10), "abcdefghij");
    }
  }
  g_main_loop_unref(loop);
}
}  // namespace {}

TYPED_TEST(HttpFetcherTest, SimpleRedirectTest) {
  if (this->IsMock())
    return;
  typename TestFixture::HttpServer server;
  ASSERT_TRUE(server.started_);
  for (size_t c = 0; c < arraysize(kRedirectCodes); ++c) {
    const string url = base::StringPrintf("/redirect/%d/medium",
                                          kRedirectCodes[c]);
    RedirectTest(true, url, this->NewLargeFetcher());
  }
}

TYPED_TEST(HttpFetcherTest, MaxRedirectTest) {
  if (this->IsMock())
    return;
  typename TestFixture::HttpServer server;
  ASSERT_TRUE(server.started_);
  string url;
  for (int r = 0; r < LibcurlHttpFetcher::kMaxRedirects; r++) {
    url += base::StringPrintf("/redirect/%d",
                              kRedirectCodes[r % arraysize(kRedirectCodes)]);
  }
  url += "/medium";
  RedirectTest(true, url, this->NewLargeFetcher());
}

TYPED_TEST(HttpFetcherTest, BeyondMaxRedirectTest) {
  if (this->IsMock())
    return;
  typename TestFixture::HttpServer server;
  ASSERT_TRUE(server.started_);
  string url;
  for (int r = 0; r < LibcurlHttpFetcher::kMaxRedirects + 1; r++) {
    url += base::StringPrintf("/redirect/%d",
                              kRedirectCodes[r % arraysize(kRedirectCodes)]);
  }
  url += "/medium";
  RedirectTest(false, url, this->NewLargeFetcher());
}

namespace {
class MultiHttpFetcherTestDelegate : public HttpFetcherDelegate {
 public:
  MultiHttpFetcherTestDelegate(int expected_response_code)
      : expected_response_code_(expected_response_code) {}
  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* bytes, int length) {
    EXPECT_EQ(fetcher, fetcher_.get());
    data.append(bytes, length);
  }
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) {
    EXPECT_EQ(fetcher, fetcher_.get());
    EXPECT_EQ(expected_response_code_ != 0, successful);
    if (expected_response_code_ != 0)
      EXPECT_EQ(expected_response_code_, fetcher->http_response_code());
    // Destroy the fetcher (because we're allowed to).
    fetcher_.reset(NULL);
    g_main_loop_quit(loop_);
  }
  virtual void TransferTerminated(HttpFetcher* fetcher) {
    ADD_FAILURE();
  }
  scoped_ptr<HttpFetcher> fetcher_;
  int expected_response_code_;
  string data;
  GMainLoop* loop_;
};

void MultiTest(HttpFetcher* fetcher_in,
               const string& url,
               const MultiHttpFetcher<LibcurlHttpFetcher>::RangesVect& ranges,
               const string& expected_prefix,
               off_t expected_size,
               int expected_response_code) {
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    MultiHttpFetcherTestDelegate delegate(expected_response_code);
    delegate.loop_ = loop;
    delegate.fetcher_.reset(fetcher_in);
    MultiHttpFetcher<LibcurlHttpFetcher>* multi_fetcher =
        dynamic_cast<MultiHttpFetcher<LibcurlHttpFetcher>*>(fetcher_in);
    ASSERT_TRUE(multi_fetcher);
    multi_fetcher->set_ranges(ranges);
    multi_fetcher->SetConnectionAsExpensive(false);
    multi_fetcher->SetBuildType(false);
    multi_fetcher->set_delegate(&delegate);

    StartTransferArgs start_xfer_args = {multi_fetcher, url};

    g_timeout_add(0, StartTransfer, &start_xfer_args);
    g_main_loop_run(loop);

    EXPECT_EQ(expected_size, delegate.data.size());
    EXPECT_EQ(expected_prefix,
              string(delegate.data.data(), expected_prefix.size()));
  }
  g_main_loop_unref(loop);
}
}  // namespace {}

TYPED_TEST(HttpFetcherTest, MultiHttpFetcherSimpleTest) {
  if (!this->IsMulti())
    return;
  typename TestFixture::HttpServer server;
  ASSERT_TRUE(server.started_);

  MultiHttpFetcher<LibcurlHttpFetcher>::RangesVect ranges;
  ranges.push_back(make_pair(0, 25));
  ranges.push_back(make_pair(99, -1));
  MultiTest(this->NewLargeFetcher(),
            this->BigUrl(),
            ranges,
            "abcdefghijabcdefghijabcdejabcdefghijabcdef",
            kBigSize - (99 - 25),
            206);
}

TYPED_TEST(HttpFetcherTest, MultiHttpFetcherLengthLimitTest) {
  if (!this->IsMulti())
    return;
  typename TestFixture::HttpServer server;
  ASSERT_TRUE(server.started_);

  MultiHttpFetcher<LibcurlHttpFetcher>::RangesVect ranges;
  ranges.push_back(make_pair(0, 24));
  MultiTest(this->NewLargeFetcher(),
            this->BigUrl(),
            ranges,
            "abcdefghijabcdefghijabcd",
            24,
            200);
}

TYPED_TEST(HttpFetcherTest, MultiHttpFetcherMultiEndTest) {
  if (!this->IsMulti())
    return;
  typename TestFixture::HttpServer server;
  ASSERT_TRUE(server.started_);

  MultiHttpFetcher<LibcurlHttpFetcher>::RangesVect ranges;
  ranges.push_back(make_pair(kBigSize - 2, -1));
  ranges.push_back(make_pair(kBigSize - 3, -1));
  MultiTest(this->NewLargeFetcher(),
            this->BigUrl(),
            ranges,
            "ijhij",
            5,
            206);
}

TYPED_TEST(HttpFetcherTest, MultiHttpFetcherInsufficientTest) {
  if (!this->IsMulti())
    return;
  typename TestFixture::HttpServer server;
  ASSERT_TRUE(server.started_);

  MultiHttpFetcher<LibcurlHttpFetcher>::RangesVect ranges;
  ranges.push_back(make_pair(kBigSize - 2, 4));
  for (int i = 0; i < 2; ++i) {
    MultiTest(this->NewLargeFetcher(),
              this->BigUrl(),
              ranges,
              "ij",
              2,
              0);
    ranges.push_back(make_pair(0, 5));
  }
}

namespace {
class BlockedTransferTestDelegate : public HttpFetcherDelegate {
 public:
  virtual void ReceivedBytes(HttpFetcher* fetcher,
                             const char* bytes, int length) {
    ADD_FAILURE();
  }
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) {
    EXPECT_FALSE(successful);
    g_main_loop_quit(loop_);
  }
  virtual void TransferTerminated(HttpFetcher* fetcher) {
    ADD_FAILURE();
  }
  GMainLoop* loop_;
};

}  // namespace

TYPED_TEST(HttpFetcherTest, BlockedTransferTest) {
  if (this->IsMock() || this->IsMulti())
    return;

  for (int i = 0; i < 2; i++) {
    typename TestFixture::HttpServer server;

    ASSERT_TRUE(server.started_);

    GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
    BlockedTransferTestDelegate delegate;
    delegate.loop_ = loop;

    scoped_ptr<HttpFetcher> fetcher(this->NewLargeFetcher());
    LibcurlHttpFetcher* curl_fetcher =
        dynamic_cast<LibcurlHttpFetcher*>(fetcher.get());
    bool is_expensive_connection = (i == 0);
    bool is_official_build = (i == 1);
    LOG(INFO) << "is_expensive_connection: " << is_expensive_connection;
    LOG(INFO) << "is_official_build: " << is_official_build;
    curl_fetcher->SetConnectionAsExpensive(is_expensive_connection);
    curl_fetcher->SetBuildType(is_official_build);
    fetcher->set_delegate(&delegate);

    StartTransferArgs start_xfer_args =
        { fetcher.get(), LocalServerUrlForPath(this->SmallUrl()) };

    g_timeout_add(0, StartTransfer, &start_xfer_args);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
  }
}

}  // namespace chromeos_update_engine
