// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STRINGS_STRING_SPLIT_H_
#define STRINGS_STRING_SPLIT_H_

#include <string>
#include <vector>

namespace strings {

// Splits |str| into a vector of strings delimited by |c|, placing the results
// in |r|. If several instances of |c| are contiguous, or if |str| begins with
// or ends with |c|, then an empty string is inserted.
//
// Every substring is trimmed of any leading or trailing white space.
// Note: |c| must be in the ASCII range.
void SplitString(const std::string& str, char c,
                 std::vector<std::string>* r);

// The same as SplitString, but use a substring delimiter instead of a char.
void SplitStringUsingSubstr(const std::string& str, const std::string& s,
                            std::vector<std::string>* r);

// The same as SplitString, but don't trim white space.
// Note: |c| must be in the ASCII range.
void SplitStringDontTrim(const std::string& str, char c,
                         std::vector<std::string>* r);

// WARNING: this uses whitespace as defined by the HTML5 spec. If you need
// a function similar to this but want to trim all types of whitespace, then
// factor this out into a function that takes a string containing the characters
// that are treated as whitespace.
//
// Splits the string along whitespace (where whitespace is the five space
// characters defined by HTML 5). Each contiguous block of non-whitespace
// characters is added to result.
void SplitStringAlongWhitespace(const std::string& str,
                                std::vector<std::string>* result);

}  // namespace strings

#endif  // STRINGS_STRING_SPLIT_H_
