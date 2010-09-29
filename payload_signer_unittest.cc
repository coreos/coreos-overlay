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

namespace chromeos_update_engine {

const char* kUnittestPrivateKeyPath = "unittest_key.pem";

//class PayloadSignerTest : public ::testing::Test {};

TEST(PayloadSignerTest, SimpleTest) {
  // Some data and its corresponding signature:
  const string kDataToSign = "This is some data to sign.";
  const char kExpectedSignature[] = {
    0xa4, 0xbc, 0x8f, 0xeb, 0x81, 0x05, 0xaa, 0x56,
    0x1b, 0x56, 0xe5, 0xcb, 0x9b, 0x1a, 0x00, 0xd7,
    0x1d, 0x87, 0x8e, 0xda, 0x5e, 0x90, 0x09, 0xb8,
    0x15, 0xf4, 0x25, 0x97, 0x2f, 0x3c, 0xa1, 0xf3,
    0x02, 0x75, 0xcd, 0x67, 0x4b, 0x0c, 0x1f, 0xf5,
    0x6e, 0xf1, 0x58, 0xd7, 0x0d, 0x8c, 0x18, 0x91,
    0x52, 0x30, 0x98, 0x64, 0x58, 0xc0, 0xe2, 0xb5,
    0x77, 0x3b, 0x96, 0x8f, 0x05, 0xc4, 0x7f, 0x7a,
    0x9a, 0x44, 0x0f, 0xc7, 0x1b, 0x90, 0x83, 0xf8,
    0x69, 0x05, 0xa8, 0x02, 0x57, 0xcd, 0x2e, 0x5b,
    0x96, 0xc7, 0x77, 0xa6, 0x1f, 0x97, 0x97, 0x05,
    0xb3, 0x30, 0x1c, 0x27, 0xd7, 0x2d, 0x31, 0x60,
    0x84, 0x7e, 0x99, 0x00, 0xe6, 0xe1, 0x39, 0xa6,
    0xf3, 0x3a, 0x72, 0xba, 0xc4, 0xfe, 0x68, 0xa9,
    0x08, 0xfa, 0xbc, 0xa8, 0x44, 0x66, 0xa0, 0x60,
    0xde, 0xc9, 0xb2, 0xba, 0xbc, 0x80, 0xb5, 0x55
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
