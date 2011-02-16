// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/proxy_resolver.h"

using std::deque;
using std::string;

namespace chromeos_update_engine {

const char kNoProxy[] = "direct://";

DirectProxyResolver::~DirectProxyResolver() {
  if (idle_callback_id_) {
    g_source_remove(idle_callback_id_);
    idle_callback_id_ = 0;
  }
}

bool DirectProxyResolver::GetProxiesForUrl(const std::string& url,
                                           ProxiesResolvedFn callback,
                                           void* data) {
  google::protobuf::Closure* closure =
      google::protobuf::NewCallback(this,
                                    &DirectProxyResolver::ReturnCallback,
                                    callback,
                                    data);
  idle_callback_id_ = g_idle_add(utils::GlibRunClosure, closure);
  return true;
}

void DirectProxyResolver::ReturnCallback(ProxiesResolvedFn callback,
                                         void* data) {
  idle_callback_id_ = 0;
  std::deque<std::string> proxies;
  proxies.push_back(kNoProxy);
  (*callback)(proxies, data);
}


}  // namespace chromeos_update_engine
