// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include "glog/logging.h"
#include "update_engine/omaha_hash_calculator.h"

namespace chromeos_update_engine {

OmahaHashCalculator::OmahaHashCalculator() {
  CHECK_EQ(1, SHA1_Init(&ctx_));
}

// Update is called with all of the data that should be hashed in order.
// Mostly just passes the data through to OpenSSL's SHA1_Update()
void OmahaHashCalculator::Update(const char* data, size_t length) {
  CHECK(hash_.empty()) << "Can't Update after hash is finalized";
  COMPILE_ASSERT(sizeof(size_t) <= sizeof(unsigned long),
                 length_param_may_be_truncated_in_SHA1_Update);
  CHECK_EQ(1, SHA1_Update(&ctx_, data, length));
}

// Call Finalize() when all data has been passed in. This mostly just
// calls OpenSSL's SHA1_Final() and then base64 encodes the hash.
void OmahaHashCalculator::Finalize() {
  CHECK(hash_.empty()) << "Don't call Finalize() twice";
  unsigned char md[SHA_DIGEST_LENGTH];
  CHECK_EQ(1, SHA1_Final(md, &ctx_));

  // Convert md to base64 encoding and store it in hash_
  BIO *b64 = BIO_new(BIO_f_base64());
  CHECK(b64);
  BIO *bmem = BIO_new(BIO_s_mem());
  CHECK(bmem);
  b64 = BIO_push(b64, bmem);
  CHECK_EQ(sizeof(md), BIO_write(b64, md, sizeof(md)));
  CHECK_EQ(1, BIO_flush(b64));

  BUF_MEM *bptr = NULL;
  BIO_get_mem_ptr(b64, &bptr);
  hash_.assign(bptr->data, bptr->length - 1);

  BIO_free_all(b64);
}

std::string OmahaHashCalculator::OmahaHashOfBytes(
    const void* data, size_t length) {
  OmahaHashCalculator calc;
  calc.Update(reinterpret_cast<const char*>(data), length);
  calc.Finalize();
  return calc.hash();
}

std::string OmahaHashCalculator::OmahaHashOfString(
    const std::string& str) {
  return OmahaHashOfBytes(str.data(), str.size());
}

std::string OmahaHashCalculator::OmahaHashOfData(
    const std::vector<char>& data) {
  return OmahaHashOfBytes(&data[0], data.size());
}

}  // namespace chromeos_update_engine
