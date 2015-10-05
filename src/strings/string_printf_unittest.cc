// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "strings/string_printf.h"

#include <errno.h>
#include <gtest/gtest.h>

namespace strings {

namespace {

// A helper for the StringAppendV test that follows.
//
// Just forwards its args to StringAppendV.
static void StringAppendVTestHelper(std::string* out, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  StringAppendV(out, format, ap);
  va_end(ap);
}

}  // namespace

TEST(StringPrintfTest, StringPrintfEmpty) {
  EXPECT_EQ("", StringPrintf("%s", ""));
}

TEST(StringPrintfTest, StringPrintfMisc) {
  EXPECT_EQ("123hello w", StringPrintf("%3d%2s %1c", 123, "hello", 'w'));
}

TEST(StringPrintfTest, StringAppendVEmptyString) {
  std::string value("Hello");
  StringAppendVTestHelper(&value, "%s", "");
  EXPECT_EQ("Hello", value);
}

TEST(StringPrintfTest, StringAppendVString) {
  std::string value("Hello");
  StringAppendVTestHelper(&value, " %s", "World");
  EXPECT_EQ("Hello World", value);
}

TEST(StringPrintfTest, StringAppendVInt) {
  std::string value("Hello");
  StringAppendVTestHelper(&value, " %d", 123);
  EXPECT_EQ("Hello 123", value);
}

TEST(StringPrintfTest, StringAppendV) {
  std::string out;
  StringAppendVTestHelper(&out, "%d foo %s", 1, "bar");
  EXPECT_EQ("1 foo bar", out);
}

// Test that StringPrintf and StringAppendV do not change errno.
TEST(StringPrintfTest, StringPrintfErrno) {
  errno = 1;
  EXPECT_EQ("", StringPrintf("%s", ""));
  EXPECT_EQ(1, errno);
  std::string out;
  StringAppendVTestHelper(&out, "%d foo %s", 1, "bar");
  EXPECT_EQ(1, errno);
}

}  // namespace strings
