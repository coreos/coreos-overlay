// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "strings/string_printf.h"

#include <stdio.h>
#include <stdlib.h>

#include <glog/logging.h>

namespace strings {

std::string StringPrintf(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  std::string result;
  StringAppendV(&result, format, ap);
  va_end(ap);
  return result;
}

std::string StringPrintV(const char* format, va_list ap) {
  std::string result;
  StringAppendV(&result, format, ap);
  return result;
}

void StringAppendV(std::string* dst, const char* format, va_list ap) {
  char *buf;
  int len = vasprintf(&buf, format, ap);
  PCHECK(len >= 0) << "vsnprintf failured";
  dst->append(buf, len);
  free(buf);
  return;
}

}  // namespace strings
