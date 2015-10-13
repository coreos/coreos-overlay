// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "strings/string_number_conversions.h"

#include <limits.h>

#include <gtest/gtest.h>

#include "macros.h"
#include "strings/string_printf.h"

namespace strings {

TEST(StringNumberConversionsTest, StringToInt) {
  static const struct {
    std::string input;
    int output;
    bool success;
  } cases[] = {
    {"0", 0, true},
    {"42", 42, true},
    {"42\x99", 42, false},
    {"\x99" "42\x99", 0, false},
    {"-2147483648", INT_MIN, true},
    {"2147483647", INT_MAX, true},
    {"", 0, false},
    {" 42", 42, false},
    {"42 ", 42, false},
    {"\t\n\v\f\r 42", 42, false},
    {"blah42", 0, false},
    {"42blah", 42, false},
    {"blah42blah", 0, false},
    {"-273.15", -273, false},
    {"+98.6", 98, false},
    {"--123", 0, false},
    {"++123", 0, false},
    {"-+123", 0, false},
    {"+-123", 0, false},
    {"-", 0, false},
    {"-2147483649", INT_MIN, false},
    {"-99999999999", INT_MIN, false},
    {"2147483648", INT_MAX, false},
    {"99999999999", INT_MAX, false},
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    int output = 0;
    EXPECT_EQ(cases[i].success, StringToInt(cases[i].input, &output));
    EXPECT_EQ(cases[i].output, output);
  }

  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] = "6\06";
  std::string input_string(input, arraysize(input) - 1);
  int output;
  EXPECT_FALSE(StringToInt(input_string, &output));
  EXPECT_EQ(6, output);
}

TEST(StringNumberConversionsTest, StringToUint) {
  static const struct {
    std::string input;
    unsigned output;
    bool success;
  } cases[] = {
    {"0", 0, true},
    {"42", 42, true},
    {"42\x99", 42, false},
    {"\x99" "42\x99", 0, false},
    {"-2147483648", 0, false},
    {"2147483647", INT_MAX, true},
    {"", 0, false},
    {" 42", 42, false},
    {"42 ", 42, false},
    {"\t\n\v\f\r 42", 42, false},
    {"blah42", 0, false},
    {"42blah", 42, false},
    {"blah42blah", 0, false},
    {"-273.15", 0, false},
    {"+98.6", 98, false},
    {"--123", 0, false},
    {"++123", 0, false},
    {"-+123", 0, false},
    {"+-123", 0, false},
    {"-", 0, false},
    {"-2147483649", 0, false},
    {"-99999999999", 0, false},
    {"4294967295", UINT_MAX, true},
    {"4294967296", UINT_MAX, false},
    {"99999999999", UINT_MAX, false},
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    unsigned output = 0;
    EXPECT_EQ(cases[i].success, StringToUint(cases[i].input, &output));
    EXPECT_EQ(cases[i].output, output);
  }

  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] = "6\06";
  std::string input_string(input, arraysize(input) - 1);
  unsigned output;
  EXPECT_FALSE(StringToUint(input_string, &output));
  EXPECT_EQ(6U, output);
}

TEST(StringNumberConversionsTest, StringToInt64) {
  static const struct {
    std::string input;
    int64_t output;
    bool success;
  } cases[] = {
    {"0", 0, true},
    {"42", 42, true},
    {"-2147483648", INT_MIN, true},
    {"2147483647", INT_MAX, true},
    {"-2147483649", INT64_C(-2147483649), true},
    {"-99999999999", INT64_C(-99999999999), true},
    {"2147483648", INT64_C(2147483648), true},
    {"99999999999", INT64_C(99999999999), true},
    {"9223372036854775807", INT64_MAX, true},
    {"-9223372036854775808", INT64_MIN, true},
    {"09", 9, true},
    {"-09", -9, true},
    {"", 0, false},
    {" 42", 42, false},
    {"42 ", 42, false},
    {"0x42", 0, false},
    {"\t\n\v\f\r 42", 42, false},
    {"blah42", 0, false},
    {"42blah", 42, false},
    {"blah42blah", 0, false},
    {"-273.15", -273, false},
    {"+98.6", 98, false},
    {"--123", 0, false},
    {"++123", 0, false},
    {"-+123", 0, false},
    {"+-123", 0, false},
    {"-", 0, false},
    {"-9223372036854775809", INT64_MIN, false},
    {"-99999999999999999999", INT64_MIN, false},
    {"9223372036854775808", INT64_MAX, false},
    {"99999999999999999999", INT64_MAX, false},
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    int64_t output = 0;
    EXPECT_EQ(cases[i].success, StringToInt64(cases[i].input, &output));
    EXPECT_EQ(cases[i].output, output);
  }

  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] = "6\06";
  std::string input_string(input, arraysize(input) - 1);
  int64_t output;
  EXPECT_FALSE(StringToInt64(input_string, &output));
  EXPECT_EQ(6, output);
}

TEST(StringNumberConversionsTest, HexEncode) {
  std::string hex(HexEncode(NULL, 0));
  EXPECT_EQ(hex.length(), 0U);
  unsigned char bytes[] = {0x01, 0xff, 0x02, 0xfe, 0x03, 0x80, 0x81};
  hex = HexEncode(bytes, sizeof(bytes));
  EXPECT_EQ(hex.compare("01FF02FE038081"), 0);
}

}  // namespace strings
