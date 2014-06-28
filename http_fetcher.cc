// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/http_fetcher.h"

namespace chromeos_update_engine {

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

}  // namespace chromeos_update_engine
