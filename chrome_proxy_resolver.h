// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_CHROME_PROXY_RESOLVER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_CHROME_PROXY_RESOLVER_H__

#include <string>
#include <vector>

#include <curl/curl.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/dbus_interface.h"
#include "update_engine/proxy_resolver.h"

namespace chromeos_update_engine {

extern const char kSessionManagerService[];
extern const char kSessionManagerPath[];
extern const char kSessionManagerInterface[];
extern const char kSessionManagerRetrievePropertyMethod[];
extern const char kSessionManagerProxySettingsKey[];

// Class to resolve proxy for a url based on Chrome's proxy settings.

// Currently only supports manual settings, not PAC files or autodetected
// settings.

class ChromeProxyResolver : public ProxyResolver {
 public:
  explicit ChromeProxyResolver(DbusGlibInterface* dbus) : dbus_(dbus) {}
  virtual ~ChromeProxyResolver() {}

  virtual bool GetProxiesForUrl(const std::string& url,
                                std::vector<std::string>* out_proxies);

  // Get the curl proxy type for a given proxy url. Returns true on success.
  // Note: if proxy is kNoProxy, this will return false.
  static bool GetProxyType(const std::string& proxy,
                              curl_proxytype* out_type);

 private:
  FRIEND_TEST(ChromeProxyResolverTest, GetProxiesForUrlWithSettingsTest);

  // Fetches a dbus proxy to session manager. Returns NULL on failure.
  DBusGProxy* DbusProxy();

  // Fetches the json-encoded proxy settings string from the session manager.
  bool GetJsonProxySettings(DBusGProxy* proxy, std::string* out_json);

  // Given a |url| and the json encoded settings |json_settings|,
  // returns the proper proxy servers in |out_proxies|. Returns true on
  // success.
  bool GetProxiesForUrlWithSettings(const std::string& url,
                                    const std::string& json_settings,
                                    std::vector<std::string>* out_proxies);

  DbusGlibInterface* dbus_;
  
  DISALLOW_COPY_AND_ASSIGN(ChromeProxyResolver);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_CHROME_PROXY_RESOLVER_H__
