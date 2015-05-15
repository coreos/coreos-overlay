// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Copyright (c) 2015 The CoreOS Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "strings/string_number_conversions.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

namespace strings {

bool StringToInt(const std::string& input, int *output) {
  const char *start = input.c_str();
  char *end;
  long o;

  errno = 0;
  o = strtol(start, &end, 10);
  // Check bounds for when long is 64 bits
  if (o < INT_MIN) {
    *output = INT_MIN;
    return false;
  }
  else if (o > INT_MAX) {
    *output = INT_MAX;
    return false;
  }
  else
    *output = o;

  if (errno)
    return false;

  // Check length instead of *end != '\0' in case input contains extra nulls.
  size_t l = end - start;
  if (l != input.length())
    return false;

  // Do not tolerate extra leading chars like strtol does.
  if (!isdigit(*start) && *start != '-')
    return false;

  return true;
}

bool StringToUint(const std::string& input, unsigned int *output) {
  const char *start = input.c_str();

  // strtoul accepts negative numbers as valid, we do not.
  for (size_t i = 0; i < input.length(); i++) {
    if (isspace(start[i]))
      continue;
    else if (start[i] == '-') {
      *output = 0;
      return false;
    }
    else
      break;
  }

  errno = 0;
  char *end;
  unsigned long o = strtoul(start, &end, 10);
  // Check bounds for when long is 64 bits
  if (o > UINT_MAX) {
    *output = UINT_MAX;
    return false;
  }
  else
    *output = o;

  if (errno)
    return false;

  // Check length instead of *end != '\0' in case input contains extra nulls.
  size_t l = end - start;
  if (l != input.length())
    return false;

  // Do not tolerate extra leading chars like strtol does.
  if (!isdigit(*start))
    return false;

  return true;
}

bool StringToInt64(const std::string& input, int64_t* output) {
  const char *start = input.c_str();
  char *end;

  errno = 0;
  *output = strtoll(start, &end, 10);
  if (errno)
    return false;

  // Check length instead of *end != '\0' in case input contains extra nulls.
  size_t l = end - start;
  if (l != input.length())
    return false;

  // Do not tolerate extra leading chars like strtol does.
  if (!isdigit(*start) && *start != '-')
    return false;

  return true;
}

std::string HexEncode(const void* bytes, size_t size) {
  static const char kHexChars[] = "0123456789ABCDEF";

  // Each input byte creates two output hex characters.
  std::string ret(size * 2, '\0');

  for (size_t i = 0; i < size; ++i) {
    char b = reinterpret_cast<const char*>(bytes)[i];
    ret[(i * 2)] = kHexChars[(b >> 4) & 0xf];
    ret[(i * 2) + 1] = kHexChars[b & 0xf];
  }
  return ret;
}

}  // namespace base
