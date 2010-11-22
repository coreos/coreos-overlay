// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/proxy_resolver.h"

using std::deque;
using std::string;

namespace chromeos_update_engine {

const char kNoProxy[] = "direct://";

bool DirectProxyResolver::GetProxiesForUrl(const string& url,
                                           deque<string>* out_proxies) {
  out_proxies->clear();
  out_proxies->push_back(kNoProxy);
  return true;
}

}  // namespace chromeos_update_engine
