// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/chrome_proxy_resolver.h"

#include <base/json/json_reader.h>
#include <base/scoped_ptr.h>
#include <base/values.h>

#include "update_engine/utils.h"

using google::protobuf::Closure;
using google::protobuf::NewCallback;
using std::deque;
using std::string;
using std::vector;

namespace chromeos_update_engine {

const char kSessionManagerService[] = "org.chromium.SessionManager";
const char kSessionManagerPath[] = "/org/chromium/SessionManager";
const char kSessionManagerInterface[] = "org.chromium.SessionManagerInterface";
const char kSessionManagerRetrievePropertyMethod[] =
    "RetrieveProperty";
const char kSessionManagerProxySettingsKey[] = "cros.proxy.everywhere";

bool ChromeProxyResolver::GetProxiesForUrl(
    const std::string& url,
    ProxiesResolvedFn callback,
    void* data) {
  ChromeProxyResolverClosureArgs args;
  args.url = url;
  args.callback = callback;
  args.data = data;
  Closure* closure = NewCallback(this,
                                 &ChromeProxyResolver::GetProxiesForUrlCallback,
                                 args);
  g_idle_add(utils::GlibRunClosure, closure);
  return true;
}

void ChromeProxyResolver::GetProxiesForUrlCallback(
    ChromeProxyResolverClosureArgs args) {
  deque<string> proxies;
  // First, query dbus for the currently stored settings
  DBusGProxy* proxy = DbusProxy();
  if (proxy) {
    string json_settings;
    if (GetJsonProxySettings(proxy, &json_settings)) {
      LOG(INFO) << "got settings:" << json_settings;
      GetProxiesForUrlWithSettings(args.url, json_settings, &proxies);
    }
  }
  (*args.callback)(proxies, args.data);
}

bool ChromeProxyResolver::GetJsonProxySettings(DBusGProxy* proxy,
                                               std::string* out_json) {
  gchar* value = NULL;
  GArray* sig = NULL;
  GError* error = NULL;
  TEST_AND_RETURN_FALSE(
      dbus_->ProxyCall(proxy,
                       kSessionManagerRetrievePropertyMethod,
                       &error,
                       G_TYPE_STRING, kSessionManagerProxySettingsKey,
                       G_TYPE_INVALID,
                       G_TYPE_STRING, &value,
                       DBUS_TYPE_G_UCHAR_ARRAY, &sig,
                       G_TYPE_INVALID));
  g_array_free(sig, false);
  out_json->assign(value);
  g_free(value);
  return true;
}

DBusGProxy* ChromeProxyResolver::DbusProxy() {
  GError* error = NULL;
  DBusGConnection* bus = dbus_->BusGet(DBUS_BUS_SYSTEM, &error);
  TEST_AND_RETURN_FALSE(bus);
  DBusGProxy* proxy = dbus_->ProxyNewForNameOwner(bus,
                                                  kSessionManagerService,
                                                  kSessionManagerPath,
                                                  kSessionManagerInterface,
                                                  &error);
  if (!proxy) {
    LOG(ERROR) << "Error getting FlimFlam proxy: "
               << utils::GetGErrorMessage(error);
  }
  return proxy;
}

namespace {
enum ProxyMode {
  kProxyModeDirect = 0,
  kProxyModeAutoDetect,
  kProxyModePACScript,
  kProxyModeSingle,
  kProxyModeProxyPerScheme
}; 
}  // namespace {}

bool ChromeProxyResolver::GetProxiesForUrlWithSettings(
    const string& url,
    const string& json_settings,
    std::deque<std::string>* out_proxies) {
  base::JSONReader parser;

  scoped_ptr<Value> root(
      parser.JsonToValue(json_settings,
                         true,  // check root is obj/arr
                         false));  // false = disallow trailing comma
  if (!root.get()) {
    LOG(ERROR) << "Unable to parse \"" << json_settings << "\": "
               << parser.GetErrorMessage();
    return false;
  }

  TEST_AND_RETURN_FALSE(root->IsType(Value::TYPE_DICTIONARY));

  DictionaryValue* root_dict = dynamic_cast<DictionaryValue*>(root.get());
  TEST_AND_RETURN_FALSE(root_dict);
  int mode = -1;
  TEST_AND_RETURN_FALSE(root_dict->GetInteger("mode", &mode));

  LOG(INFO) << "proxy mode: " << mode;
  if (mode != kProxyModeSingle &&
      mode != kProxyModeProxyPerScheme) {
    LOG(INFO) << "unsupported proxy scheme";
    out_proxies->clear();
    out_proxies->push_back(kNoProxy);
    return true;
  }
  if (mode == kProxyModeSingle) {
    LOG(INFO) << "single proxy mode";
    string proxy_string;
    TEST_AND_RETURN_FALSE(root_dict->GetString("single.server", &proxy_string));
    if (proxy_string.find("://") == string::npos) {
      // missing protocol, assume http.
      proxy_string = string("http://") + proxy_string;
    }
    out_proxies->clear();
    out_proxies->push_back(proxy_string);
    LOG(INFO) << "single proxy: " << (*out_proxies)[0];
    out_proxies->push_back(kNoProxy);
    return true;
  }
  // Proxy per scheme mode.
  LOG(INFO) << "proxy per scheme mode";

  // Find which scheme we are
  bool url_is_http = utils::StringHasPrefix(url, "http://");
  if (!url_is_http)
    TEST_AND_RETURN_FALSE(utils::StringHasPrefix(url, "https://"));

  // Using "proto_*" variables to refer to http or https
  const string proto_path = url_is_http ? "http.server" : "https.server";
  const string socks_path = "socks.server";

  out_proxies->clear();

  string proto_server, socks_server;
  if (root_dict->GetString(proto_path, &proto_server)) {
    if (proto_server.find("://") == string::npos) {
      // missing protocol, assume http.
      proto_server = string("http://") + proto_server;
    }
    out_proxies->push_back(proto_server);
    LOG(INFO) << "got http/https server: " << proto_server;
  }
  if (root_dict->GetString(socks_path, &socks_server)) {
    out_proxies->push_back(socks_server);
    LOG(INFO) << "got socks server: " << proto_server;
  }
  out_proxies->push_back(kNoProxy);
  return true;
}

bool ChromeProxyResolver::GetProxyType(const std::string& proxy,
                                       curl_proxytype* out_type) {
  if (utils::StringHasPrefix(proxy, "socks5://") ||
      utils::StringHasPrefix(proxy, "socks://")) {
    *out_type = CURLPROXY_SOCKS5_HOSTNAME;
    return true;
  }
  if (utils::StringHasPrefix(proxy, "socks4://")) {
    *out_type = CURLPROXY_SOCKS4A;
    return true;
  }
  if (utils::StringHasPrefix(proxy, "http://") ||
      utils::StringHasPrefix(proxy, "https://")) {
    *out_type = CURLPROXY_HTTP;
    return true;
  }
  if (utils::StringHasPrefix(proxy, kNoProxy)) {
    // known failure case. don't log.
    return false;
  }
  LOG(INFO) << "Unknown proxy type: " << proxy;
  return false;
}

}  // namespace chromeos_update_engine
