// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "update_engine/bzip.h"
#include "update_engine/gzip.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

template <typename T>
class ZipTest : public ::testing::Test {
 public:
  bool ZipDecompress(const std::vector<char>& in,
                     std::vector<char>* out) const = 0;
  bool ZipCompress(const std::vector<char>& in,
                   std::vector<char>* out) const = 0;
  bool ZipCompressString(const std::string& str,
                         std::vector<char>* out) const = 0;
  bool ZipDecompressString(const std::string& str,
                           std::vector<char>* out) const = 0;
};

class GzipTest {};

template <>
class ZipTest<GzipTest> : public ::testing::Test {
 public:
  bool ZipDecompress(const std::vector<char>& in,
                     std::vector<char>* out) const {
    return GzipDecompress(in, out);
  }
  bool ZipCompress(const std::vector<char>& in,
                   std::vector<char>* out) const {
    return GzipCompress(in, out);
  }
  bool ZipCompressString(const std::string& str,
                         std::vector<char>* out) const {
    return GzipCompressString(str, out);
  }
  bool ZipDecompressString(const std::string& str,
                           std::vector<char>* out) const {
    return GzipDecompressString(str, out);
  }
};

class BzipTest {};

template <>
class ZipTest<BzipTest> : public ::testing::Test {
 public:
  bool ZipDecompress(const std::vector<char>& in,
                     std::vector<char>* out) const {
    return BzipDecompress(in, out);
  }
  bool ZipCompress(const std::vector<char>& in,
                   std::vector<char>* out) const {
    return BzipCompress(in, out);
  }
  bool ZipCompressString(const std::string& str,
                         std::vector<char>* out) const {
    return BzipCompressString(str, out);
  }
  bool ZipDecompressString(const std::string& str,
                           std::vector<char>* out) const {
    return BzipDecompressString(str, out);
  }
};

typedef ::testing::Types<GzipTest, BzipTest>
    ZipTestTypes;
TYPED_TEST_CASE(ZipTest, ZipTestTypes);



TYPED_TEST(ZipTest, SimpleTest) {
  string in("this should compress well xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
  vector<char> out;
  EXPECT_TRUE(this->ZipCompressString(in, &out));
  EXPECT_LT(out.size(), in.size());
  EXPECT_GT(out.size(), 0);
  vector<char> decompressed;
  EXPECT_TRUE(this->ZipDecompress(out, &decompressed));
  EXPECT_EQ(in.size(), decompressed.size());
  EXPECT_TRUE(!memcmp(in.data(), &decompressed[0], in.size()));
}

TYPED_TEST(ZipTest, PoorCompressionTest) {
  string in(reinterpret_cast<const char*>(kRandomString),
            sizeof(kRandomString));
  vector<char> out;
  EXPECT_TRUE(this->ZipCompressString(in, &out));
  EXPECT_GT(out.size(), in.size());
  string out_string(&out[0], out.size());
  vector<char> decompressed;
  EXPECT_TRUE(this->ZipDecompressString(out_string, &decompressed));
  EXPECT_EQ(in.size(), decompressed.size());
  EXPECT_TRUE(!memcmp(in.data(), &decompressed[0], in.size()));
}

TYPED_TEST(ZipTest, MalformedZipTest) {
  string in(reinterpret_cast<const char*>(kRandomString),
            sizeof(kRandomString));
  vector<char> out;
  EXPECT_FALSE(this->ZipDecompressString(in, &out));
}

TYPED_TEST(ZipTest, EmptyInputsTest) {
  string in;
  vector<char> out;
  EXPECT_TRUE(this->ZipDecompressString(in, &out));
  EXPECT_EQ(0, out.size());

  EXPECT_TRUE(this->ZipCompressString(in, &out));
  EXPECT_EQ(0, out.size());
}

}  // namespace chromeos_update_engine
