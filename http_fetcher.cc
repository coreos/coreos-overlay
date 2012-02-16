// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/http_fetcher.h"

using google::protobuf::Closure;
using std::deque;
using std::string;

namespace chromeos_update_engine {

HttpFetcher::~HttpFetcher() {
  if (no_resolver_idle_id_) {
    g_source_remove(no_resolver_idle_id_);
    no_resolver_idle_id_ = 0;
  }
}

void HttpFetcher::SetPostData(const void* data, size_t size,
                              HttpContentType type) {
  post_data_set_ = true;
  post_data_.clear();
  const char *char_data = reinterpret_cast<const char*>(data);
  post_data_.insert(post_data_.end(), char_data, char_data + size);
  post_content_type_ = type;
}

void HttpFetcher::SetPostData(const void* data, size_t size) {
  SetPostData(data, size, kHttpContentTypeUnspecified);
}

// Proxy methods to set the proxies, then to pop them off.
bool HttpFetcher::ResolveProxiesForUrl(const string& url, Closure* callback) {
  if (!proxy_resolver_) {
    LOG(INFO) << "Not resolving proxies (no proxy resolver).";
    no_resolver_idle_id_ = g_idle_add(utils::GlibRunClosure, callback);
    return true;
  }
  CHECK_EQ(reinterpret_cast<Closure*>(NULL), callback_);
  callback_ = callback;
  return proxy_resolver_->GetProxiesForUrl(url,
                                           &HttpFetcher::StaticProxiesResolved,
                                           this);
}

void HttpFetcher::ProxiesResolved(const std::deque<std::string>& proxies) {
  no_resolver_idle_id_ = 0;
  if (!proxies.empty())
    SetProxies(proxies);
  CHECK_NE(reinterpret_cast<Closure*>(NULL), callback_);
  Closure* callback = callback_;
  callback_ = NULL;
  // This may indirectly call back into ResolveProxiesForUrl():
  callback->Run();
}

}  // namespace chromeos_update_engine
