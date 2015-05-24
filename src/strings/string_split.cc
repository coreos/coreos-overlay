// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "strings/string_split.h"

#include <utility>

#include <base/string_util.h>

namespace strings {

namespace {

std::vector<std::string> Split(const std::string& str,
                               const std::string& delim,
                               bool trim_whitespace) {
  std::vector<std::string> r;
  size_t begin_index = 0;
  while (true) {
    size_t end_index = str.find(delim, begin_index);
    if (end_index == std::string::npos) {
      std::string tmp = str.substr(begin_index);
      if (trim_whitespace)
        TrimWhitespace(tmp, TRIM_ALL, &tmp);
      // Avoid converting an empty or all-whitespace source string into a vector
      // of one empty string.
      if (!r.empty() || !tmp.empty())
        r.push_back(std::move(tmp));
      return r;
    }
    std::string tmp = str.substr(begin_index, end_index - begin_index);
    if (trim_whitespace)
      TrimWhitespace(tmp, TRIM_ALL, &tmp);
    r.push_back(std::move(tmp));
    begin_index = end_index + delim.size();
  }
}

}  // namespace

void SplitStringAlongWhitespace(const std::string& str,
                                std::vector<std::string>* result) {
  result->clear();
  const size_t length = str.length();
  if (!length)
    return;

  bool last_was_ws = false;
  size_t last_non_ws_start = 0;
  for (size_t i = 0; i < length; ++i) {
    switch (str[i]) {
      // HTML 5 defines whitespace as: space, tab, LF, line tab, FF, or CR.
      case L' ':
      case L'\t':
      case L'\xA':
      case L'\xB':
      case L'\xC':
      case L'\xD':
        if (!last_was_ws) {
          if (i > 0) {
            result->push_back(
                str.substr(last_non_ws_start, i - last_non_ws_start));
          }
          last_was_ws = true;
        }
        break;

      default:  // Not a space character.
        if (last_was_ws) {
          last_was_ws = false;
          last_non_ws_start = i;
        }
        break;
    }
  }
  if (!last_was_ws) {
    result->push_back(
        str.substr(last_non_ws_start, length - last_non_ws_start));
  }
}

std::vector<std::string> SplitAndTrim(const std::string& str,
                                      const std::string &delim) {
  return Split(str, delim, true);
}

std::vector<std::string> SplitAndTrim(const std::string& str, char delim) {
  return Split(str, std::string{delim}, true);
}

std::vector<std::string> SplitDontTrim(const std::string& str, char delim) {
  return Split(str, std::string{delim}, false);
}

}  // namespace strings
