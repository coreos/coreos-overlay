// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_PROXY_RESOLVER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_PROXY_RESOLVER_H__

#include <base/logging.h>

#include <deque>
#include <string>

namespace chromeos_update_engine {

extern const char kNoProxy[];

class ProxyResolver {
 public:
  ProxyResolver() {}
  virtual ~ProxyResolver() {}

  // Stores a list of proxies for a given |url| in |out_proxy|.
  // Returns true on success. The resultant proxy will be in one of the
  // following forms:
  // http://<host>[:<port>] - HTTP proxy
  // socks{4,5}://<host>[:<port>] - SOCKS4/5 proxy
  // kNoProxy - no proxy
  virtual bool GetProxiesForUrl(const std::string& url,
                                std::deque<std::string>* out_proxies) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyResolver);
};

// Always says to not use a proxy
class DirectProxyResolver : public ProxyResolver {
 public:
  DirectProxyResolver() {}
  virtual bool GetProxiesForUrl(const std::string& url,
                                std::deque<std::string>* out_proxies);

 private:
  DISALLOW_COPY_AND_ASSIGN(DirectProxyResolver);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_PROXY_RESOLVER_H__
