// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>
#include <base/scoped_ptr.h>
#include <glib.h>
#include <glog/logging.h>
#include <gtest/gtest.h>
#include "update_engine/libcurl_http_fetcher.h"
#include "update_engine/mock_http_fetcher.h"

namespace chromeos_update_engine {

namespace {
// WARNING, if you update this, you must also update test_http_server.py
const char* const kServerPort = "8080";
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
  typedef NullHttpServer HttpServer;
};

class PythonHttpServer {
 public:
  PythonHttpServer() {
    char *argv[2] = {strdup("./test_http_server.py"), NULL};
    GError *err;
    started_ = false;
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
    while (0 != rc) {
      
      rc = system((string("wget --output-document=/dev/null ") +
                   LocalServerUrlForPath("/test")).c_str());
      usleep(10 * 1000);  // 10 ms
    }
    started_ = true;
    free(argv[0]);
    return;
  }
  ~PythonHttpServer() {
    if (!started_)
      return;
    // request that the server exit itself
    system((string("wget --output-document=/dev/null ") +
            LocalServerUrlForPath("/quitquitquit")).c_str());
    waitpid(pid_, NULL, 0);
  }
  GPid pid_;
  bool started_;
};

template <>
class HttpFetcherTest<LibcurlHttpFetcher> : public ::testing::Test {
 public:
  HttpFetcher* NewLargeFetcher() {
    LibcurlHttpFetcher *ret = new LibcurlHttpFetcher;
    ret->set_idle_ms(1);  // speeds up test execution
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
  typedef PythonHttpServer HttpServer;
};

typedef ::testing::Types<LibcurlHttpFetcher, MockHttpFetcher>
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
    g_main_loop_quit(loop_);
  }
  GMainLoop* loop_;
};
}  // namespace {}

TYPED_TEST(HttpFetcherTest, SimpleTest) {
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    HttpFetcherTestDelegate delegate;
    delegate.loop_ = loop;
    scoped_ptr<HttpFetcher> fetcher(this->NewSmallFetcher());
    fetcher->set_delegate(&delegate);

    typename TestFixture::HttpServer server;
    ASSERT_TRUE(server.started_);

    fetcher->BeginTransfer(this->SmallUrl());
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
    LOG(INFO) << "got " << length << " bytes";
    memset(str, 0, length + 1);
    memcpy(str, bytes, length);
    CHECK(!paused_);
    paused_ = true;
    fetcher->Pause();
    LOG(INFO) << "calling pause";
  }
  virtual void TransferComplete(HttpFetcher* fetcher, bool successful) {
    g_main_loop_quit(loop_);
  }
  void Unpause() {
    CHECK(paused_);
    paused_ = false;
    fetcher_->Unpause();
    LOG(INFO) << "calling unpause";
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
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);
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
    CHECK(false);  // We should never get here
    g_main_loop_quit(loop_);
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
  HttpFetcher* fetcher_;
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
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);
  {
    AbortingHttpFetcherTestDelegate delegate;
    scoped_ptr<HttpFetcher> fetcher(this->NewLargeFetcher());
    delegate.once_ = true;
    delegate.loop_ = loop;
    delegate.fetcher_ = fetcher.get();
    fetcher->set_delegate(&delegate);

    typename TestFixture::HttpServer server;
    ASSERT_TRUE(server.started_);
    GSource* timeout_source_;
    timeout_source_ = g_timeout_source_new(0);  // ms
    g_source_set_callback(timeout_source_, AbortingTimeoutCallback, &delegate,
                          NULL);
    g_source_attach(timeout_source_, NULL);
    fetcher->BeginTransfer(this->BigUrl());

    g_main_loop_run(loop);
    g_source_destroy(timeout_source_);
  }
  g_main_loop_unref(loop);
}

}  // namespace chromeos_update_engine
