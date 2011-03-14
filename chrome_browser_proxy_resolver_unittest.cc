// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <string>

#include <gtest/gtest.h>

#include "update_engine/chrome_browser_proxy_resolver.h"
#include "update_engine/mock_dbus_interface.h"

using std::deque;
using std::string;
using ::testing::_;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;

namespace chromeos_update_engine {

class ChromeBrowserProxyResolverTest : public ::testing::Test { };

TEST(ChromeBrowserProxyResolverTest, ParseTest) {
  // Test ideas from
  // http://src.chromium.org/svn/trunk/src/net/proxy/proxy_list_unittest.cc
  const char* inputs[] = {
    "PROXY foopy:10",
    " DIRECT",  // leading space.
    "PROXY foopy1 ; proxy foopy2;\t DIRECT",
    "proxy foopy1 ; SOCKS foopy2",
    "DIRECT ; proxy foopy1 ; DIRECT ; SOCKS5 foopy2;DIRECT ",
    "DIRECT ; proxy foopy1:80; DIRECT ; DIRECT",
    "PROXY-foopy:10",
    "PROXY",
    "PROXY foopy1 ; JUNK ; JUNK ; SOCKS5 foopy2 ; ;",
    "HTTP foopy1; SOCKS5 foopy2"
  };
  deque<string> outputs[arraysize(inputs)];
  outputs[0].push_back("http://foopy:10");
  outputs[0].push_back(kNoProxy);
  outputs[1].push_back(kNoProxy);
  outputs[2].push_back("http://foopy1");
  outputs[2].push_back("http://foopy2");
  outputs[2].push_back(kNoProxy);
  outputs[3].push_back("http://foopy1");
  outputs[3].push_back("socks4://foopy2");
  outputs[3].push_back(kNoProxy);
  outputs[4].push_back(kNoProxy);
  outputs[4].push_back("http://foopy1");
  outputs[4].push_back(kNoProxy);
  outputs[4].push_back("socks5://foopy2");
  outputs[4].push_back(kNoProxy);
  outputs[5].push_back(kNoProxy);
  outputs[5].push_back("http://foopy1:80");
  outputs[5].push_back(kNoProxy);
  outputs[5].push_back(kNoProxy);
  outputs[6].push_back(kNoProxy);
  outputs[7].push_back(kNoProxy);
  outputs[8].push_back("http://foopy1");
  outputs[8].push_back("socks5://foopy2");
  outputs[8].push_back(kNoProxy);
  outputs[9].push_back("socks5://foopy2");
  outputs[9].push_back(kNoProxy);

  for (size_t i = 0; i < arraysize(inputs); i++) {
    deque<string> results =
        ChromeBrowserProxyResolver::ParseProxyString(inputs[i]);
    deque<string>& expected = outputs[i];
    EXPECT_EQ(results.size(), expected.size()) << "i = " << i;
    if (expected.size() != results.size())
      continue;
    for (size_t j = 0; j < expected.size(); j++) {
      EXPECT_EQ(expected[j], results[j]) << "i = " << i;
    }
  }
}

namespace {
void DbusInterfaceTestResolved(const std::deque<std::string>& proxies,
                               void* data) {
  EXPECT_EQ(2, proxies.size());
  EXPECT_EQ("socks5://192.168.52.83:5555", proxies[0]);
  EXPECT_EQ(kNoProxy, proxies[1]);
  g_main_loop_quit(reinterpret_cast<GMainLoop*>(data));
}
void DbusInterfaceTestResolvedNoReply(const std::deque<std::string>& proxies,
                                      void* data) {
  EXPECT_EQ(1, proxies.size());
  EXPECT_EQ(kNoProxy, proxies[0]);
  g_main_loop_quit(reinterpret_cast<GMainLoop*>(data));
}
struct SendReplyArgs {
  DBusConnection* connection;
  DBusMessage* message;
  ChromeBrowserProxyResolver* resolver;
};
gboolean SendReply(gpointer data) {
  LOG(INFO) << "Calling SendReply";
  SendReplyArgs* args = reinterpret_cast<SendReplyArgs*>(data);
  ChromeBrowserProxyResolver::StaticFilterMessage(
      args->connection,
      args->message,
      args->resolver);
  return FALSE;  // Don't keep calling this function
}

// chrome_replies should be set to whether or not we fake a reply from
// chrome. If there's no reply, the resolver should time out.
// If chrome_alive is false, assume that sending to chrome fails.
void RunTest(bool chrome_replies, bool chrome_alive) {
  long number = 1;
  DBusGConnection* kMockSystemGBus =
      reinterpret_cast<DBusGConnection*>(number++);
  DBusConnection* kMockSystemBus =
      reinterpret_cast<DBusConnection*>(number++);
  DBusGProxy* kMockDbusProxy =
      reinterpret_cast<DBusGProxy*>(number++);
  DBusMessage* kMockDbusMessage =
      reinterpret_cast<DBusMessage*>(number++);

  const char kUrl[] = "http://example.com/blah";

  MockDbusGlib dbus_iface;
  
  EXPECT_CALL(dbus_iface, BusGet(_, _))
      .Times(chrome_alive ? 3 : 2)
      .WillRepeatedly(Return(kMockSystemGBus));
  EXPECT_CALL(dbus_iface,
              ConnectionGetConnection(kMockSystemGBus))
      .Times(chrome_alive ? 2 : 1)
      .WillRepeatedly(Return(kMockSystemBus));
  EXPECT_CALL(dbus_iface, DbusBusAddMatch(kMockSystemBus, _, _));
  EXPECT_CALL(dbus_iface,
              DbusConnectionAddFilter(kMockSystemBus, _, _, _))
      .WillOnce(Return(1));
  EXPECT_CALL(dbus_iface,
              ProxyNewForNameOwner(kMockSystemGBus,
                                   StrEq(kLibCrosServiceName),
                                   StrEq(kLibCrosServicePath),
                                   StrEq(kLibCrosServiceInterface),
                                   _))
      .WillOnce(Return(chrome_alive ? kMockDbusProxy : NULL));
  if (chrome_alive)
    EXPECT_CALL(dbus_iface, ProxyCall(
        kMockDbusProxy,
        StrEq(kLibCrosServiceResolveNetworkProxyMethodName),
        _,
        G_TYPE_STRING, StrEq(kUrl),
        G_TYPE_STRING, StrEq(kLibCrosProxyResolveSignalInterface),
        G_TYPE_STRING, StrEq(kLibCrosProxyResolveName),
        G_TYPE_INVALID))
        .WillOnce(Return(chrome_alive ? TRUE : FALSE));
  EXPECT_CALL(dbus_iface,
              DbusConnectionRemoveFilter(kMockSystemBus, _, _));
  if (chrome_replies) {
    EXPECT_CALL(dbus_iface,
                DbusMessageIsSignal(kMockDbusMessage,
                                    kLibCrosProxyResolveSignalInterface,
                                    kLibCrosProxyResolveName))
        .WillOnce(Return(1));
    EXPECT_CALL(dbus_iface,
                DbusMessageGetArgs(kMockDbusMessage, _,
                                   DBUS_TYPE_STRING, _,
                                   DBUS_TYPE_STRING, _,
                                   DBUS_TYPE_STRING, _,
                                   DBUS_TYPE_INVALID))
        .WillOnce(DoAll(SetArgumentPointee<3>(strdup(kUrl)),
                        SetArgumentPointee<5>(
                            strdup("SOCKS5 192.168.52.83:5555;DIRECT")),
                        Return(TRUE)));
  }

  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);

  ChromeBrowserProxyResolver resolver(&dbus_iface);
  EXPECT_EQ(chrome_alive, resolver.Init());
  resolver.set_timeout(1);
  SendReplyArgs args = {
    kMockSystemBus,
    kMockDbusMessage,
    &resolver
  };
  if (chrome_replies)
    g_idle_add(SendReply, &args);
  EXPECT_TRUE(resolver.GetProxiesForUrl(kUrl,
                                        chrome_replies ?
                                        &DbusInterfaceTestResolved :
                                        &DbusInterfaceTestResolvedNoReply,
                                        loop));
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
}
}  // namespace {}

TEST(ChromeBrowserProxyResolverTest, SuccessTest) {
  RunTest(true, true);
}

TEST(ChromeBrowserProxyResolverTest, NoReplyTest) {
  RunTest(false, true);
}

TEST(ChromeBrowserProxyResolverTest, NoChromeTest) {
  RunTest(false, false);
}

}  // namespace chromeos_update_engine
