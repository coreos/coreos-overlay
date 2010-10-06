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
const char* kUnittestPublicKeyPath = "unittest_key.pub.pem";

// Some data and its corresponding hash and signature:
const char kDataToSign[] = "This is some data to sign.";
const char kDataHash[] = {
  0x7a, 0x07, 0xa6, 0x44, 0x08, 0x86, 0x20, 0xa6,
  0xc1, 0xf8, 0xd9, 0x02, 0x05, 0x63, 0x0d, 0xb7,
  0xfc, 0x2b, 0xa0, 0xa9, 0x7c, 0x9d, 0x1d, 0x8c,
  0x01, 0xf5, 0x78, 0x6d, 0xc5, 0x11, 0xb4, 0x06
};
const char kDataSignature[] = {
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

//class PayloadSignerTest : public ::testing::Test {};

namespace {
void SignSampleData(vector<char>* out_signature_blob) {
  string data_path;
  ASSERT_TRUE(
      utils::MakeTempFile("/tmp/data.XXXXXX", &data_path, NULL));
  ScopedPathUnlinker data_path_unlinker(data_path);
  ASSERT_TRUE(utils::WriteFile(data_path.c_str(),
                               kDataToSign,
                               strlen(kDataToSign)));
  uint64_t length = 0;
  EXPECT_TRUE(PayloadSigner::SignatureBlobLength(kUnittestPrivateKeyPath,
                                                 &length));
  EXPECT_GT(length, 0);
  EXPECT_TRUE(PayloadSigner::SignPayload(data_path,
                                         kUnittestPrivateKeyPath,
                                         out_signature_blob));
  EXPECT_EQ(length, out_signature_blob->size());
}
}

TEST(PayloadSignerTest, SimpleTest) {
  vector<char> signature_blob;
  SignSampleData(&signature_blob);

  // Check the signature itself
  Signatures signatures;
  EXPECT_TRUE(signatures.ParseFromArray(&signature_blob[0],
                                        signature_blob.size()));
  EXPECT_EQ(1, signatures.signatures_size());
  const Signatures_Signature& signature = signatures.signatures(0);
  EXPECT_EQ(kSignatureMessageVersion, signature.version());
  const string sig_data = signature.data();
  ASSERT_EQ(arraysize(kDataSignature), sig_data.size());
  for (size_t i = 0; i < arraysize(kDataSignature); i++) {
    EXPECT_EQ(kDataSignature[i], sig_data[i]);
  }
}

TEST(PayloadSignerTest, RunAsRootVerifySignatureTest) {
  vector<char> signature_blob;
  SignSampleData(&signature_blob);

  vector<char> hash_data;
  EXPECT_TRUE(PayloadSigner::VerifySignature(signature_blob,
                                             kUnittestPublicKeyPath,
                                             &hash_data));
  ASSERT_EQ(arraysize(kDataHash), hash_data.size());
  for (size_t i = 0; i < arraysize(kDataHash); i++) {
    EXPECT_EQ(kDataHash[i], hash_data[i]);
  }
}

}  // namespace chromeos_update_engine
