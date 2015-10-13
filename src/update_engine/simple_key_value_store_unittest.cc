// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include <gtest/gtest.h>

#include "macros.h"
#include "update_engine/simple_key_value_store.h"

using std::map;
using std::string;

namespace chromeos_update_engine {

class SimpleKeyValueStoreTest : public ::testing::Test {};

TEST(SimpleKeyValueStoreTest, SimpleTest) {
  string blob = "A=B\nC=\n=\nFOO=BAR=BAZ\nBAR=BAX\nMISSING=NEWLINE";
  map<string, string> parts = simple_key_value_store::ParseString(blob);
  string combined = simple_key_value_store::AssembleString(parts);
  map<string, string> combined_parts =
      simple_key_value_store::ParseString(combined);
  map<string, string>* maps[] = { &parts, &combined_parts };
  for (size_t i = 0; i < arraysize(maps); i++) {
    map<string, string>* test_map = maps[i];
    EXPECT_EQ(6, test_map->size()) << "i = " << i;
    EXPECT_EQ("B", (*test_map)["A"]) << "i = " << i;
    EXPECT_EQ("", (*test_map)["C"]) << "i = " << i;
    EXPECT_EQ("", (*test_map)[""]) << "i = " << i;
    EXPECT_EQ("BAR=BAZ", (*test_map)["FOO"]) << "i = " << i;
    EXPECT_EQ("BAX", (*test_map)["BAR"]) << "i = " << i;
    EXPECT_EQ("NEWLINE", (*test_map)["MISSING"]) << "i = " << i;
  }
}

TEST(SimpleKeyValueStoreTest, EmptyTest) {
  map<string, string> empty_map = simple_key_value_store::ParseString("");
  EXPECT_TRUE(empty_map.empty());
  string str = simple_key_value_store::AssembleString(empty_map);
  if (!str.empty())
    EXPECT_EQ("\n", str);  // Optionally may end in newline
}

}  // namespace chromeos_update_engine
