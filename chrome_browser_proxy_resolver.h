// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_CHROME_BROWSER_PROXY_RESOLVER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_CHROME_BROWSER_PROXY_RESOLVER_H__

#include <map>
#include <string>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/dbus_interface.h"
#include "update_engine/proxy_resolver.h"

namespace chromeos_update_engine {

extern const char kLibCrosServiceName[];
extern const char kLibCrosServicePath[];
extern const char kLibCrosServiceInterface[];
extern const char kLibCrosServiceResolveNetworkProxyMethodName[];
extern const char kLibCrosProxyResolveName[];
extern const char kLibCrosProxyResolveSignalInterface[];
extern const char kLibCrosProxyResolveSignalFilter[];

class ChromeBrowserProxyResolver : public ProxyResolver {
 public:
  explicit ChromeBrowserProxyResolver(DbusGlibInterface* dbus);
  virtual ~ChromeBrowserProxyResolver();
  bool Init();

  virtual bool GetProxiesForUrl(const std::string& url,
                                ProxiesResolvedFn callback,
                                void* data);
  void set_timeout(int seconds) { timeout_ = seconds; }

  // Public for test
  static DBusHandlerResult StaticFilterMessage(
      DBusConnection* connection,
      DBusMessage* message,
      void* data) {
    return reinterpret_cast<ChromeBrowserProxyResolver*>(data)->FilterMessage(
        connection, message);
  }

 private:
  FRIEND_TEST(ChromeBrowserProxyResolverTest, ParseTest);
  FRIEND_TEST(ChromeBrowserProxyResolverTest, SuccessTest);
  typedef std::multimap<std::string, std::pair<ProxiesResolvedFn, void*> >
    CallbacksMap;
  typedef std::multimap<std::string, GSource*> TimeoutsMap;

  // Handle a reply from Chrome:
  void HandleReply(const std::string& source_url,
                   const std::string& proxy_list);
  DBusHandlerResult FilterMessage(
      DBusConnection* connection,
      DBusMessage* message);
  // Handle no reply:
  void HandleTimeout(std::string source_url);
  
  // Parses a string-encoded list of proxies and returns a deque
  // of individual proxies. The last one will always be kNoProxy.
  static std::deque<std::string> ParseProxyString(const std::string& input);
  
  // Deletes internal state for the first instance of url in the state.
  // If delete_timer is set, calls g_source_destroy on the timer source.
  // Returns the callback in an out parameter. Returns true on success.
  bool DeleteUrlState(const std::string& url,
                      bool delete_timer,
                      std::pair<ProxiesResolvedFn, void*>* callback);

  // Shutdown the dbus proxy object.
  void Shutdown();

  DbusGlibInterface* dbus_;
  DBusGProxy* proxy_;
  DBusGProxy* peer_proxy_;
  int timeout_;
  TimeoutsMap timers_;
  CallbacksMap callbacks_;
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserProxyResolver);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_CHROME_BROWSER_PROXY_RESOLVER_H__
