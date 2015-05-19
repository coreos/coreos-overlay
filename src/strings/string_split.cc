// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "strings/string_split.h"

#include <algorithm>
#include <utility>

namespace strings {

namespace {

const std::string kWhitespaceASCII = {
  0x09,    // CHARACTER TABULATION
  0x0A,    // LINE FEED (LF)
  0x0B,    // LINE TABULATION
  0x0C,    // FORM FEED (FF)
  0x0D,    // CARRIAGE RETURN (CR)
  0x20,    // SPACE
};

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
        tmp = TrimWhitespace(tmp);
      // Avoid converting an empty or all-whitespace source string into a vector
      // of one empty string.
      if (!r.empty() || !tmp.empty())
        r.push_back(std::move(tmp));
      return r;
    }
    std::string tmp = str.substr(begin_index, end_index - begin_index);
    if (trim_whitespace)
      tmp = TrimWhitespace(tmp);
    r.push_back(std::move(tmp));
    begin_index = end_index + delim.size();
  }
}

std::string Trim(const std::string& input, const std::string& trim_chars) {
  size_t first_good_char = input.find_first_not_of(trim_chars);
  size_t last_good_char = input.find_last_not_of(trim_chars);

  if (first_good_char == std::string::npos)
    first_good_char = input.length();

  return input.substr(first_good_char, last_good_char - first_good_char + 1);
}

}  // namespace

std::vector<std::string> SplitWords(const std::string& str) {
  std::vector<std::string> result;

  for (auto it = str.begin(); it != str.end();) {
    auto first = std::find_if(it, str.end(), [](char c) {
      return kWhitespaceASCII.find(c) == std::string::npos;
    });
    auto last = std::find_if(first, str.end(), [](char c) {
      return kWhitespaceASCII.find(c) != std::string::npos;
    });
    if (first != last)
      result.push_back(std::string(first, last));

    it = last;
  }

  return result;
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

std::string TrimWhitespace(const std::string& input) {
  return Trim(input, kWhitespaceASCII);
}

}  // namespace strings
