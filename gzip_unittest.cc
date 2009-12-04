// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "update_engine/gzip.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

class GzipTest : public ::testing::Test { };

TEST(GzipTest, SimpleTest) {
  string in("this should compress well xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
  vector<char> out;
  EXPECT_TRUE(GzipCompressString(in, &out));
  EXPECT_LT(out.size(), in.size());
  EXPECT_GT(out.size(), 0);
  vector<char> decompressed;
  EXPECT_TRUE(GzipDecompress(out, &decompressed));
  EXPECT_EQ(in.size(), decompressed.size());
  EXPECT_TRUE(!memcmp(in.data(), &decompressed[0], in.size()));
}

TEST(GzipTest, PoorCompressionTest) {
  string in(reinterpret_cast<const char*>(kRandomString),
            sizeof(kRandomString));
  vector<char> out;
  EXPECT_TRUE(GzipCompressString(in, &out));
  EXPECT_GT(out.size(), in.size());
  string out_string(&out[0], out.size());
  vector<char> decompressed;
  EXPECT_TRUE(GzipDecompressString(out_string, &decompressed));
  EXPECT_EQ(in.size(), decompressed.size());
  EXPECT_TRUE(!memcmp(in.data(), &decompressed[0], in.size()));
}

TEST(GzipTest, MalformedGzipTest) {
  string in(reinterpret_cast<const char*>(kRandomString),
            sizeof(kRandomString));
  vector<char> out;
  EXPECT_FALSE(GzipDecompressString(in, &out));
}

TEST(GzipTest, EmptyInputsTest) {
  string in;
  vector<char> out;
  EXPECT_TRUE(GzipDecompressString(in, &out));
  EXPECT_EQ(0, out.size());

  EXPECT_TRUE(GzipCompressString(in, &out));
  EXPECT_EQ(0, out.size());
}

}  // namespace chromeos_update_engine
