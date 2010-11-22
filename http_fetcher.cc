// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/http_fetcher.h"

using std::deque;
using std::string;

namespace chromeos_update_engine {

void HttpFetcher::SetPostData(const void* data, size_t size) {
  post_data_set_ = true;
  post_data_.clear();
  const char *char_data = reinterpret_cast<const char*>(data);
  post_data_.insert(post_data_.end(), char_data, char_data + size);
}

// Proxy methods to set the proxies, then to pop them off.
void HttpFetcher::ResolveProxiesForUrl(const string& url) {
  if (!proxy_resolver_) {
    LOG(INFO) << "Not resolving proxies (no proxy resolver).";
    return;
  }
  deque<string> proxies;
  if (proxy_resolver_->GetProxiesForUrl(url, &proxies)) {
    SetProxies(proxies);
  }
}

}  // namespace chromeos_update_engine
