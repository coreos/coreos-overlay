// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "strings/string_split.h"

#include <gmock/gmock.h>  // for EXPECT_THAT
#include <gtest/gtest.h>

#include "macros.h"

using ::testing::ElementsAre;

namespace strings {

TEST(StringSplitTest, SplitAndTrimChar) {
  std::vector<std::string> r;

  r = SplitAndTrim(std::string(), ',');
  EXPECT_EQ(0U, r.size());

  r = SplitAndTrim("a,b,c", ',');
  ASSERT_EQ(3U, r.size());
  EXPECT_EQ(r[0], "a");
  EXPECT_EQ(r[1], "b");
  EXPECT_EQ(r[2], "c");

  r = SplitAndTrim("a, b, c", ',');
  ASSERT_EQ(3U, r.size());
  EXPECT_EQ(r[0], "a");
  EXPECT_EQ(r[1], "b");
  EXPECT_EQ(r[2], "c");

  r = SplitAndTrim("a,,c", ',');
  ASSERT_EQ(3U, r.size());
  EXPECT_EQ(r[0], "a");
  EXPECT_EQ(r[1], "");
  EXPECT_EQ(r[2], "c");

  r = SplitAndTrim("   ", '*');
  EXPECT_EQ(0U, r.size());

  r = SplitAndTrim("foo", '*');
  ASSERT_EQ(1U, r.size());
  EXPECT_EQ(r[0], "foo");

  r = SplitAndTrim("foo ,", ',');
  ASSERT_EQ(2U, r.size());
  EXPECT_EQ(r[0], "foo");
  EXPECT_EQ(r[1], "");

  r = SplitAndTrim(",", ',');
  ASSERT_EQ(2U, r.size());
  EXPECT_EQ(r[0], "");
  EXPECT_EQ(r[1], "");

  r = SplitAndTrim("\t\ta\t", '\t');
  ASSERT_EQ(4U, r.size());
  EXPECT_EQ(r[0], "");
  EXPECT_EQ(r[1], "");
  EXPECT_EQ(r[2], "a");
  EXPECT_EQ(r[3], "");

  r = SplitAndTrim("\ta\t\nb\tcc", '\n');
  ASSERT_EQ(2U, r.size());
  EXPECT_EQ(r[0], "a");
  EXPECT_EQ(r[1], "b\tcc");
}

TEST(StringSplitTest, SplitAndTrimEmptyString) {
  std::vector<std::string> results = SplitAndTrim(
      std::string(), "DELIMITER");
  ASSERT_EQ(0u, results.size());
}

TEST(StringSplitTest, SplitAndTrimNoDelimiter) {
  std::vector<std::string> results = SplitAndTrim(
      "alongwordwithnodelimiter", "DELIMITER");
  ASSERT_EQ(1u, results.size());
  EXPECT_THAT(results, ElementsAre("alongwordwithnodelimiter"));
}

TEST(StringSplitTest, SplitAndTrimLeadingDelimitersSkipped) {
  std::vector<std::string> results = SplitAndTrim(
      "DELIMITERDELIMITERDELIMITERoneDELIMITERtwoDELIMITERthree",
      "DELIMITER");
  ASSERT_EQ(6u, results.size());
  EXPECT_THAT(results, ElementsAre("", "", "", "one", "two", "three"));
}

TEST(StringSplitTest, SplitAndTrimConsecutiveDelimitersSkipped) {
  std::vector<std::string> results = SplitAndTrim(
      "unoDELIMITERDELIMITERDELIMITERdosDELIMITERtresDELIMITERDELIMITERcuatro",
      "DELIMITER");
  ASSERT_EQ(7u, results.size());
  EXPECT_THAT(results, ElementsAre("uno", "", "", "dos", "tres", "", "cuatro"));
}

TEST(StringSplitTest, SplitAndTrimTrailingDelimitersSkipped) {
  std::vector<std::string> results = SplitAndTrim(
      "unDELIMITERdeuxDELIMITERtroisDELIMITERquatreDELIMITERDELIMITERDELIMITER",
      "DELIMITER");
  ASSERT_EQ(7u, results.size());
  EXPECT_THAT(
      results, ElementsAre("un", "deux", "trois", "quatre", "", "", ""));
}

TEST(StringSplitTest, SplitDontTrim) {
  std::vector<std::string> r;

  r = SplitDontTrim("   ", '*');
  ASSERT_EQ(1U, r.size());
  EXPECT_EQ(r[0], "   ");

  r = SplitDontTrim("\t  \ta\t ", '\t');
  ASSERT_EQ(4U, r.size());
  EXPECT_EQ(r[0], "");
  EXPECT_EQ(r[1], "  ");
  EXPECT_EQ(r[2], "a");
  EXPECT_EQ(r[3], " ");

  r = SplitDontTrim("\ta\t\nb\tcc", '\n');
  ASSERT_EQ(2U, r.size());
  EXPECT_EQ(r[0], "\ta\t");
  EXPECT_EQ(r[1], "b\tcc");
}

TEST(StringSplitTest, SplitWords) {
  struct TestData {
    const char* input;
    const size_t expected_result_count;
    const char* output1;
    const char* output2;
  } data[] = {
    { "a",       1, "a",  ""   },
    { " ",       0, "",   ""   },
    { " a",      1, "a",  ""   },
    { " ab ",    1, "ab", ""   },
    { " ab c",   2, "ab", "c"  },
    { " ab c ",  2, "ab", "c"  },
    { " ab cd",  2, "ab", "cd" },
    { " ab cd ", 2, "ab", "cd" },
    { " \ta\t",  1, "a",  ""   },
    { " b\ta\t", 2, "b",  "a"  },
    { " b\tat",  2, "b",  "at" },
    { "b\tat",   2, "b",  "at" },
    { "b\t at",  2, "b",  "at" },
  };
  for (size_t i = 0; i < arraysize(data); ++i) {
    std::vector<std::string> results = SplitWords(data[i].input);
    ASSERT_EQ(data[i].expected_result_count, results.size());
    if (data[i].expected_result_count > 0)
      ASSERT_EQ(data[i].output1, results[0]);
    if (data[i].expected_result_count > 1)
      ASSERT_EQ(data[i].output2, results[1]);
  }
}

TEST(StringSplitTest, TrimWhitespace) {
  struct {
    const char* input;
    const char* output;
  } data[] = {
    {"Google Video ", "Google Video"},
    {" Google Video", "Google Video"},
    {" Google Video ", "Google Video"},
    {"Google Video", "Google Video"},
    {"", ""},
    {"  ", ""},
    {"\t\rTest String\n", "Test String"},
  };

  for (size_t i = 0; i < arraysize(data); ++i) {
    EXPECT_EQ(data[i].output, TrimWhitespace(data[i].input));
  }
}

}  // namespace base
