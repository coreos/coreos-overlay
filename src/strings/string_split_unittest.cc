// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "strings/string_split.h"

#include <base/basictypes.h>  // for arraysize
#include <gmock/gmock.h>  // for EXPECT_THAT
#include <gtest/gtest.h>

using ::testing::ElementsAre;

namespace strings {

TEST(SplitStringUsingSubstrTest, EmptyString) {
  std::vector<std::string> results;
  SplitStringUsingSubstr(std::string(), "DELIMITER", &results);
  ASSERT_EQ(1u, results.size());
  EXPECT_THAT(results, ElementsAre(""));
}

TEST(StringSplitTest, SplitString) {
  std::vector<std::string> r;

  SplitString(std::string(), ',', &r);
  EXPECT_EQ(0U, r.size());
  r.clear();

  SplitString("a,b,c", ',', &r);
  ASSERT_EQ(3U, r.size());
  EXPECT_EQ(r[0], "a");
  EXPECT_EQ(r[1], "b");
  EXPECT_EQ(r[2], "c");
  r.clear();

  SplitString("a, b, c", ',', &r);
  ASSERT_EQ(3U, r.size());
  EXPECT_EQ(r[0], "a");
  EXPECT_EQ(r[1], "b");
  EXPECT_EQ(r[2], "c");
  r.clear();

  SplitString("a,,c", ',', &r);
  ASSERT_EQ(3U, r.size());
  EXPECT_EQ(r[0], "a");
  EXPECT_EQ(r[1], "");
  EXPECT_EQ(r[2], "c");
  r.clear();

  SplitString("   ", '*', &r);
  EXPECT_EQ(0U, r.size());
  r.clear();

  SplitString("foo", '*', &r);
  ASSERT_EQ(1U, r.size());
  EXPECT_EQ(r[0], "foo");
  r.clear();

  SplitString("foo ,", ',', &r);
  ASSERT_EQ(2U, r.size());
  EXPECT_EQ(r[0], "foo");
  EXPECT_EQ(r[1], "");
  r.clear();

  SplitString(",", ',', &r);
  ASSERT_EQ(2U, r.size());
  EXPECT_EQ(r[0], "");
  EXPECT_EQ(r[1], "");
  r.clear();

  SplitString("\t\ta\t", '\t', &r);
  ASSERT_EQ(4U, r.size());
  EXPECT_EQ(r[0], "");
  EXPECT_EQ(r[1], "");
  EXPECT_EQ(r[2], "a");
  EXPECT_EQ(r[3], "");
  r.clear();

  SplitString("\ta\t\nb\tcc", '\n', &r);
  ASSERT_EQ(2U, r.size());
  EXPECT_EQ(r[0], "a");
  EXPECT_EQ(r[1], "b\tcc");
  r.clear();
}

TEST(SplitStringUsingSubstrTest, StringWithNoDelimiter) {
  std::vector<std::string> results;
  SplitStringUsingSubstr("alongwordwithnodelimiter", "DELIMITER", &results);
  ASSERT_EQ(1u, results.size());
  EXPECT_THAT(results, ElementsAre("alongwordwithnodelimiter"));
}

TEST(SplitStringUsingSubstrTest, LeadingDelimitersSkipped) {
  std::vector<std::string> results;
  SplitStringUsingSubstr(
      "DELIMITERDELIMITERDELIMITERoneDELIMITERtwoDELIMITERthree",
      "DELIMITER",
      &results);
  ASSERT_EQ(6u, results.size());
  EXPECT_THAT(results, ElementsAre("", "", "", "one", "two", "three"));
}

TEST(SplitStringUsingSubstrTest, ConsecutiveDelimitersSkipped) {
  std::vector<std::string> results;
  SplitStringUsingSubstr(
      "unoDELIMITERDELIMITERDELIMITERdosDELIMITERtresDELIMITERDELIMITERcuatro",
      "DELIMITER",
      &results);
  ASSERT_EQ(7u, results.size());
  EXPECT_THAT(results, ElementsAre("uno", "", "", "dos", "tres", "", "cuatro"));
}

TEST(SplitStringUsingSubstrTest, TrailingDelimitersSkipped) {
  std::vector<std::string> results;
  SplitStringUsingSubstr(
      "unDELIMITERdeuxDELIMITERtroisDELIMITERquatreDELIMITERDELIMITERDELIMITER",
      "DELIMITER",
      &results);
  ASSERT_EQ(7u, results.size());
  EXPECT_THAT(
      results, ElementsAre("un", "deux", "trois", "quatre", "", "", ""));
}

TEST(StringSplitTest, StringSplitDontTrim) {
  std::vector<std::string> r;

  SplitStringDontTrim("   ", '*', &r);
  ASSERT_EQ(1U, r.size());
  EXPECT_EQ(r[0], "   ");

  SplitStringDontTrim("\t  \ta\t ", '\t', &r);
  ASSERT_EQ(4U, r.size());
  EXPECT_EQ(r[0], "");
  EXPECT_EQ(r[1], "  ");
  EXPECT_EQ(r[2], "a");
  EXPECT_EQ(r[3], " ");

  SplitStringDontTrim("\ta\t\nb\tcc", '\n', &r);
  ASSERT_EQ(2U, r.size());
  EXPECT_EQ(r[0], "\ta\t");
  EXPECT_EQ(r[1], "b\tcc");
}

TEST(StringSplitTest, SplitStringAlongWhitespace) {
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
    std::vector<std::string> results;
    SplitStringAlongWhitespace(data[i].input, &results);
    ASSERT_EQ(data[i].expected_result_count, results.size());
    if (data[i].expected_result_count > 0)
      ASSERT_EQ(data[i].output1, results[0]);
    if (data[i].expected_result_count > 1)
      ASSERT_EQ(data[i].output2, results[1]);
  }
}

}  // namespace base
