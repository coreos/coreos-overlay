// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STRINGS_STRING_SPLIT_H_
#define STRINGS_STRING_SPLIT_H_

#include <string>
#include <vector>

namespace strings {

// Splits |str| into a vector of strings delimited by |delim|. If several
// instances of |delim| are contiguous, or if |str| begins with or ends
// with |delim|, then an empty string is inserted.
//
// Every substring is trimmed of any leading or trailing white space.
std::vector<std::string> SplitAndTrim(const std::string& str, char delim);
std::vector<std::string> SplitAndTrim(const std::string& str,
                                      const std::string& delim);

// The same as SplitAndTrim, but don't trim white space.
std::vector<std::string> SplitDontTrim(const std::string& str, char delim);

// Splits the string along ASCII whitespace.
// Each contiguous block of non-whitespace characters is added to result.
std::vector<std::string> SplitWords(const std::string& str);

// Trims any ASCII whitespace from either end of the input string.
std::string TrimWhitespace(const std::string& input);

}  // namespace strings

#endif  // STRINGS_STRING_SPLIT_H_
