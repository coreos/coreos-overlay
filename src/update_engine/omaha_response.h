// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_RESPONSE_H
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_RESPONSE_H

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <vector>

namespace chromeos_update_engine {

// This struct encapsulates the data Omaha's response for the request.
// The strings in this struct are not XML escaped.
struct OmahaResponse {
  OmahaResponse()
      : update_exists(false),
        poll_interval(0),
        size(0),
        max_failure_count_per_url(0),
        needs_admin(false),
        prompt(false),
        is_delta_payload(false),
        disable_payload_backoff(false) {}

  // True iff there is an update to be downloaded.
  bool update_exists;

  // If non-zero, server-dictated poll interval in seconds.
  int poll_interval;

  // These are only valid if update_exists is true:
  std::string display_version;

  // The ordered list of URLs in the Omaha response. Each item is a complete
  // URL (i.e. in terms of Omaha XML, each value is a urlBase + packageName)
  std::vector<std::string> payload_urls;

  std::string more_info_url;
  std::string hash;
  std::string deadline;
  off_t size;
  // The number of URL-related failures to tolerate before moving on to the
  // next URL in the current pass. This is a configurable value from the
  // Omaha Response attribute, if ever we need to fine tune the behavior.
  uint32_t max_failure_count_per_url;
  bool needs_admin;
  bool prompt;

  // True if the payload described in this response is a delta payload.
  // False if it's a full payload.
  bool is_delta_payload;

  // True if the Omaha rule instructs us to disable the backoff logic
  // on the client altogether. False otherwise.
  bool disable_payload_backoff;
};
static_assert(sizeof(off_t) == 8, "off_t not 64 bit");

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_RESPONSE_H
