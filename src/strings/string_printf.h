// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STRINGS_STRING_PRINTF_H_
#define STRINGS_STRING_PRINTF_H_

#include <stdarg.h>   // va_list

#include <string>

namespace strings {

// Return a C++ string given printf-like input.
std::string StringPrintf(const char* format, ...)
    __attribute__((format(printf, 1, 2)))
    __attribute__((warn_unused_result));

// Return a C++ string given vprintf-like input.
std::string StringPrintV(const char* format, va_list ap)
    __attribute__((format(printf, 1, 0)))
    __attribute__((warn_unused_result));

// Lower-level routine that takes a va_list and appends to a specified
// string.  All other routines are just convenience wrappers around it.
void StringAppendV(std::string* dst, const char* format, va_list ap)
    __attribute__((format(printf, 2, 0)));

}  // namespace strings

#endif  // STRINGS_STRING_PRINTF_H_
