// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <string>

#include <gtest/gtest.h>

#include "update_engine/chrome_proxy_resolver.h"
#include "update_engine/mock_dbus_interface.h"

using std::deque;
using std::string;
using ::testing::_;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;

namespace chromeos_update_engine {

class ChromeProxyResolverTest : public ::testing::Test { };

TEST(ChromeProxyResolverTest, GetProxiesForUrlWithSettingsTest) {
  const string kSingle =
      "{\"mode\":3,\"single\":{\"server\":\"192.168.42.86:80\",\"src\":0}}";
  const string kSocks =
      "{\"mode\":4,\"socks\":{\"server\":\"socks5://192.168.42.83:5555\","
      "\"src\":0}}";
  const string kAll =
      "{\"http\":{\"server\":\"http_proxy:11\",\"src\":0},"
      "\"https\":{\"server\":\"https://https_proxy:22\",\"src\":0},"
      "\"mode\":4,"
      "\"socks\":{\"server\":\"socks5://socks:55\",\"src\":0}}";
  const string kHttpHttps =
      "{\"http\":{\"server\":\"http_proxy:11\",\"src\":0},"
      "\"https\":{\"server\":\"https://https_proxy:22\",\"src\":0},"
      "\"mode\":4}";

  ChromeProxyResolver resolver(NULL);
  deque<string> out;
  string urls[] = {"http://foo.com/update", "https://bar.com/foo.gz"};
  string multi_settings[] = {kAll, kHttpHttps};
  for (size_t i = 0; i < arraysize(urls); i++) {
    const string& url = urls[i];
    LOG(INFO) << "url: " << url;
    EXPECT_TRUE(resolver.GetProxiesForUrlWithSettings(url, kSingle, &out));
    EXPECT_EQ(2, out.size());
    EXPECT_EQ("http://192.168.42.86:80", out[0]);
    EXPECT_EQ(kNoProxy, out[1]);

    EXPECT_TRUE(resolver.GetProxiesForUrlWithSettings(url, kSocks, &out));
    EXPECT_EQ(2, out.size());
    EXPECT_EQ("socks5://192.168.42.83:5555", out[0]);
    EXPECT_EQ(kNoProxy, out[1]);
  
    for (size_t j = 0; j < arraysize(multi_settings); j++) {
      const string& settings = multi_settings[j];
      EXPECT_TRUE(resolver.GetProxiesForUrlWithSettings(url, settings, &out));
      EXPECT_EQ(j == 0 ? 3 : 2, out.size());
      if (i == 0)
        EXPECT_EQ("http://http_proxy:11", out[0]);
      else
        EXPECT_EQ("https://https_proxy:22", out[0]);
      if (j == 0)
        EXPECT_EQ("socks5://socks:55", out[1]);
      EXPECT_EQ(kNoProxy, out.back());
    }
  }
  
  // Bad JSON
  EXPECT_FALSE(resolver.GetProxiesForUrlWithSettings("http://foo.com",
                                                     "}",
                                                     &out));
  
  // Bad proxy scheme
  EXPECT_TRUE(resolver.GetProxiesForUrlWithSettings("http://foo.com",
                                                    "{\"mode\":1}",
                                                    &out));
  EXPECT_EQ(1, out.size());
  EXPECT_EQ(kNoProxy, out[0]);
}

namespace {
void DbusInterfaceTestResolved(const std::deque<std::string>& proxies,
                               void* data) {
  EXPECT_EQ(2, proxies.size());
  EXPECT_EQ("socks5://192.168.52.83:5555", proxies[0]);
  EXPECT_EQ(kNoProxy, proxies[1]);
  g_main_loop_quit(reinterpret_cast<GMainLoop*>(data));
}
}

TEST(ChromeProxyResolverTest, DbusInterfaceTest) {
  long number = 1;
  DBusGConnection* kMockSystemBus =
      reinterpret_cast<DBusGConnection*>(number++);
  DBusGProxy* kMockSessionManagerProxy =
      reinterpret_cast<DBusGProxy*>(number++);

  const char settings[] = 
      "{\"mode\":4,\"socks\":{\"server\":\"socks5://192.168.52.83:5555\","
      "\"src\":0}}";

  MockDbusGlib dbus_iface;
  ChromeProxyResolver resolver(&dbus_iface);

  GArray* ret_array = g_array_new(FALSE, FALSE, 1);

  EXPECT_CALL(dbus_iface, BusGet(DBUS_BUS_SYSTEM, _))
      .Times(1)
      .WillRepeatedly(Return(kMockSystemBus));
  EXPECT_CALL(dbus_iface,
              ProxyNewForNameOwner(kMockSystemBus,
                                   StrEq(kSessionManagerService),
                                   StrEq(kSessionManagerPath),
                                   StrEq(kSessionManagerInterface),
                                   _))
      .WillOnce(Return(kMockSessionManagerProxy));
  EXPECT_CALL(dbus_iface, ProxyCall(
      kMockSessionManagerProxy,
      StrEq(kSessionManagerRetrievePropertyMethod),
      _,
      G_TYPE_STRING, StrEq(kSessionManagerProxySettingsKey),
      G_TYPE_INVALID,
      G_TYPE_STRING, _,
      DBUS_TYPE_G_UCHAR_ARRAY, _))
      .WillOnce(DoAll(SetArgumentPointee<7>(strdup(settings)),
                      SetArgumentPointee<9>(ret_array),
                      Return(TRUE)));

  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);

  EXPECT_TRUE(resolver.GetProxiesForUrl("http://user:pass@foo.com:22",
                                        &DbusInterfaceTestResolved,
                                        loop));
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
}

}  // namespace chromeos_update_engine
