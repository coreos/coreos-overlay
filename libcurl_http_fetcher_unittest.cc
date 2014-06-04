// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "update_engine/libcurl_http_fetcher.h"
#include "update_engine/proxy_resolver.h"

namespace chromeos_update_engine {

class LibcurlHttpFetcherTest : public ::testing::Test { };

TEST(LibcurlHttpFetcherTest, GetProxyTypeTest) {
  curl_proxytype type;
  EXPECT_TRUE(LibcurlHttpFetcher::GetProxyType("socks://f.ru", &type));
  EXPECT_EQ(CURLPROXY_SOCKS5_HOSTNAME, type);
  EXPECT_TRUE(LibcurlHttpFetcher::GetProxyType("socks5://f.ru:9", &type));
  EXPECT_EQ(CURLPROXY_SOCKS5_HOSTNAME, type);
  EXPECT_TRUE(LibcurlHttpFetcher::GetProxyType("socks4://f.ru:9", &type));
  EXPECT_EQ(CURLPROXY_SOCKS4A, type);
  EXPECT_TRUE(LibcurlHttpFetcher::GetProxyType("http://f.no:88", &type));
  EXPECT_EQ(CURLPROXY_HTTP, type);
  EXPECT_TRUE(LibcurlHttpFetcher::GetProxyType("https://f.no:88", &type));
  EXPECT_EQ(CURLPROXY_HTTP, type);
  EXPECT_FALSE(LibcurlHttpFetcher::GetProxyType(kNoProxy, &type));
  EXPECT_FALSE(LibcurlHttpFetcher::GetProxyType("foo://", &type));
  EXPECT_FALSE(LibcurlHttpFetcher::GetProxyType("missing.com:8", &type));
}

}  // namespace chromeos_update_engine
