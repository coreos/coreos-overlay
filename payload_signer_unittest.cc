// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "base/logging.h"
#include "update_engine/payload_signer.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

// Note: the test key was generated with the following command:
// openssl genrsa -out unittest_key.pem 1024

const char kUnittestPrivateKeyPath[] = "unittest_key.pem";

namespace chromeos_update_engine {

//class PayloadSignerTest : public ::testing::Test {};

TEST(PayloadSignerTest, SimpleTest) {
  // Some data and its corresponding signature:
  const string kDataToSign = "This is some data to sign.";
  const char kExpectedSignature[] = {
    0x74, 0xd9, 0xea, 0x45, 0xf4, 0xd8, 0x64, 0x16,
    0x88, 0x1b, 0x7f, 0x8b, 0x5d, 0xcb, 0x22, 0x2c,
    0xb1, 0xce, 0x6d, 0x6d, 0x7c, 0x8f, 0x76, 0xf0,
    0xb7, 0xa9, 0x80, 0xb3, 0x5e, 0x0b, 0xdd, 0x99,
    0xfd, 0x88, 0x1f, 0x64, 0xd6, 0xac, 0x0c, 0x1b,
    0xb1, 0x3c, 0x28, 0x11, 0x97, 0x15, 0x97, 0xec,
    0x90, 0x25, 0xa0, 0x64, 0x90, 0x36, 0x5a, 0x96,
    0x21, 0xdf, 0x16, 0x42, 0x6d, 0x7c, 0xb1, 0xf2,
    0xf6, 0xe3, 0xb2, 0xa9, 0xea, 0xc8, 0xec, 0x6b,
    0xa1, 0x99, 0x8a, 0xf0, 0x25, 0x0d, 0xcd, 0x41,
    0x85, 0x76, 0x7c, 0xe1, 0xd6, 0x70, 0x71, 0xda,
    0x02, 0x9f, 0xa2, 0x40, 0xb2, 0xfe, 0xfd, 0x84,
    0x5c, 0xcf, 0x08, 0xa8, 0x50, 0x16, 0x46, 0xc1,
    0x37, 0xe1, 0x16, 0xd2, 0xf5, 0x49, 0xe3, 0xcb,
    0x58, 0x57, 0x11, 0x97, 0x49, 0x8f, 0x14, 0x1d,
    0x4d, 0xa6, 0xfc, 0x75, 0x63, 0x64, 0xa3, 0xd5
  };

  string data_path;
  ASSERT_TRUE(
      utils::MakeTempFile("/tmp/data.XXXXXX", &data_path, NULL));
  ScopedPathUnlinker data_path_unlinker(data_path);
  ASSERT_TRUE(utils::WriteFile(data_path.c_str(),
                               kDataToSign.data(),
                               kDataToSign.size()));
  uint64_t length = 0;
  EXPECT_TRUE(PayloadSigner::SignatureBlobLength(kUnittestPrivateKeyPath,
                                                 &length));
  EXPECT_GT(length, 0);
  vector<char> signature_blob;
  EXPECT_TRUE(PayloadSigner::SignPayload(data_path,
                                         kUnittestPrivateKeyPath,
                                         &signature_blob));
  EXPECT_EQ(length, signature_blob.size());

  // Check the signature itself

  Signatures signatures;
  EXPECT_TRUE(signatures.ParseFromArray(&signature_blob[0],
                                        signature_blob.size()));
  EXPECT_EQ(1, signatures.signatures_size());
  const Signatures_Signature& signature = signatures.signatures(0);
  EXPECT_EQ(kSignatureMessageVersion, signature.version());
  const string sig_data = signature.data();
  ASSERT_EQ(sizeof(kExpectedSignature), sig_data.size());
  for (size_t i = 0; i < sizeof(kExpectedSignature); i++) {
    EXPECT_EQ(kExpectedSignature[i], sig_data[i]);
  }
}

}  // namespace chromeos_update_engine
