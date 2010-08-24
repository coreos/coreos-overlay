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
    0x00, 0x8d, 0x20, 0x22, 0x87, 0xd3, 0xd0, 0xeb,
    0x85, 0x80, 0xde, 0x76, 0xa4, 0x5a, 0xac, 0xdc,
    0xa8, 0xe0, 0x6e, 0x10, 0x98, 0xc3, 0xa4, 0x55,
    0x48, 0xbf, 0x15, 0x98, 0x32, 0xda, 0xbe, 0x21,
    0x3d, 0xa8, 0x1a, 0xb6, 0xf9, 0x93, 0x03, 0x70,
    0x44, 0x1b, 0xec, 0x39, 0xe3, 0xd4, 0xfd, 0x6b,
    0xff, 0x84, 0xee, 0x60, 0xbe, 0xed, 0x9e, 0x5b,
    0xac, 0xd5, 0xd6, 0x1a, 0xf9, 0x4e, 0xdb, 0x6d,
    0x11, 0x9e, 0x01, 0xb1, 0xcb, 0x55, 0x05, 0x52,
    0x8c, 0xad, 0xb6, 0x8e, 0x9f, 0xf7, 0xc2, 0x1a,
    0x26, 0xb3, 0x96, 0xd2, 0x4a, 0xfd, 0x7c, 0x96,
    0x53, 0x38, 0x3a, 0xcf, 0xab, 0x95, 0x83, 0xbd,
    0x8e, 0xe1, 0xbd, 0x07, 0x12, 0xa2, 0x80, 0x18,
    0xca, 0x64, 0x91, 0xee, 0x9d, 0x9d, 0xe3, 0x69,
    0xc0, 0xab, 0x1b, 0x75, 0x9f, 0xf0, 0x64, 0x74,
    0x01, 0xb3, 0x49, 0xea, 0x87, 0x63, 0x04, 0x29
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
