// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "strings/string_split.h"

#include <base/logging.h>
#include <base/string_util.h>

namespace strings {

namespace {

void SplitString(const std::string& str,
                 char s,
                 bool trim_whitespace,
                 std::vector<std::string>* r) {
  DCHECK_GE(s, 0);
  DCHECK_LT(s, 0x7F);

  r->clear();
  size_t last = 0;
  size_t c = str.size();
  for (size_t i = 0; i <= c; ++i) {
    if (i == c || str[i] == s) {
      std::string tmp(str, last, i - last);
      if (trim_whitespace)
        TrimWhitespace(tmp, TRIM_ALL, &tmp);
      // Avoid converting an empty or all-whitespace source string into a vector
      // of one empty string.
      if (i != c || !r->empty() || !tmp.empty())
        r->push_back(tmp);
      last = i + 1;
    }
  }
}

}  // namespace

void SplitStringUsingSubstr(const std::string& str,
                            const std::string& s,
                            std::vector<std::string>* r) {
  r->clear();
  size_t begin_index = 0;
  while (true) {
    size_t end_index = str.find(s, begin_index);
    if (end_index == std::string::npos) {
      const std::string term = str.substr(begin_index);
      std::string tmp;
      TrimWhitespace(term, TRIM_ALL, &tmp);
      r->push_back(tmp);
      return;
    }
    const std::string term = str.substr(begin_index, end_index - begin_index);
    std::string tmp;
    TrimWhitespace(term, TRIM_ALL, &tmp);
    r->push_back(tmp);
    begin_index = end_index + s.size();
  }
}

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

void SplitString(const std::string& str,
                 char c,
                 std::vector<std::string>* r) {
  SplitString(str, c, true, r);
}

void SplitStringDontTrim(const std::string& str,
                         char c,
                         std::vector<std::string>* r) {
  SplitString(str, c, false, r);
}

}  // namespace strings
