// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/omaha_hash_calculator.h"

#include <fcntl.h>

#include <base/eintr_wrapper.h>
#include <base/logging.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>

#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

OmahaHashCalculator::OmahaHashCalculator() : valid_(false) {
  valid_ = (SHA256_Init(&ctx_) == 1);
  LOG_IF(ERROR, !valid_) << "SHA256_Init failed";
}

// Update is called with all of the data that should be hashed in order.
// Mostly just passes the data through to OpenSSL's SHA256_Update()
bool OmahaHashCalculator::Update(const char* data, size_t length) {
  TEST_AND_RETURN_FALSE(valid_);
  TEST_AND_RETURN_FALSE(hash_.empty());
  COMPILE_ASSERT(sizeof(size_t) <= sizeof(unsigned long),
                 length_param_may_be_truncated_in_SHA256_Update);
  TEST_AND_RETURN_FALSE(SHA256_Update(&ctx_, data, length) == 1);
  return true;
}

off_t OmahaHashCalculator::UpdateFile(const string& name, off_t length) {
  int fd = HANDLE_EINTR(open(name.c_str(), O_RDONLY));
  if (fd < 0) {
    return -1;
  }

  const int kBufferSize = 128 * 1024;  // 128 KiB
  vector<char> buffer(kBufferSize);
  off_t bytes_processed = 0;
  while (length < 0 || bytes_processed < length) {
    off_t bytes_to_read = buffer.size();
    if (length >= 0 && bytes_to_read > length - bytes_processed) {
      bytes_to_read = length - bytes_processed;
    }
    ssize_t rc = HANDLE_EINTR(read(fd, buffer.data(), bytes_to_read));
    if (rc == 0) {  // EOF
      break;
    }
    if (rc < 0 || !Update(buffer.data(), rc)) {
      bytes_processed = -1;
      break;
    }
    bytes_processed += rc;
  }
  HANDLE_EINTR(close(fd));
  return bytes_processed;
}

bool OmahaHashCalculator::Base64Encode(const void* data,
                                       size_t size,
                                       string* out) {
  bool success = true;
  BIO *b64 = BIO_new(BIO_f_base64());
  if (!b64)
    LOG(INFO) << "BIO_new(BIO_f_base64()) failed";
  BIO *bmem = BIO_new(BIO_s_mem());
  if (!bmem)
    LOG(INFO) << "BIO_new(BIO_s_mem()) failed";
  if (b64 && bmem) {
    b64 = BIO_push(b64, bmem);
    success =
        (BIO_write(b64, data, size) == static_cast<int>(size));
    if (success)
      success = (BIO_flush(b64) == 1);

    BUF_MEM *bptr = NULL;
    BIO_get_mem_ptr(b64, &bptr);
    out->assign(bptr->data, bptr->length - 1);
  }
  if (b64) {
    BIO_free_all(b64);
    b64 = NULL;
  }
  return success;
}

// Call Finalize() when all data has been passed in. This mostly just
// calls OpenSSL's SHA256_Final() and then base64 encodes the hash.
bool OmahaHashCalculator::Finalize() {
  TEST_AND_RETURN_FALSE(hash_.empty());
  TEST_AND_RETURN_FALSE(raw_hash_.empty());
  raw_hash_.resize(SHA256_DIGEST_LENGTH);
  TEST_AND_RETURN_FALSE(
      SHA256_Final(reinterpret_cast<unsigned char*>(&raw_hash_[0]),
                   &ctx_) == 1);

  // Convert raw_hash_ to base64 encoding and store it in hash_.
  return Base64Encode(&raw_hash_[0], raw_hash_.size(), &hash_);;
}

bool OmahaHashCalculator::RawHashOfData(const vector<char>& data,
                                        vector<char>* out_hash) {
  OmahaHashCalculator calc;
  calc.Update(&data[0], data.size());

  out_hash->resize(out_hash->size() + SHA256_DIGEST_LENGTH);
  TEST_AND_RETURN_FALSE(
      SHA256_Final(reinterpret_cast<unsigned char*>(&(*(out_hash->end() -
                                                        SHA256_DIGEST_LENGTH))),
                   &calc.ctx_) == 1);
  return true;
}

off_t OmahaHashCalculator::RawHashOfFile(const std::string& name, off_t length,
                                         std::vector<char>* out_hash) {
  OmahaHashCalculator calc;
  off_t res = calc.UpdateFile(name, length);
  if (res < 0) {
    return res;
  }
  if (!calc.Finalize()) {
    return -1;
  }
  *out_hash = calc.raw_hash();
  return res;
}

string OmahaHashCalculator::OmahaHashOfBytes(
    const void* data, size_t length) {
  OmahaHashCalculator calc;
  calc.Update(reinterpret_cast<const char*>(data), length);
  calc.Finalize();
  return calc.hash();
}

string OmahaHashCalculator::OmahaHashOfString(const string& str) {
  return OmahaHashOfBytes(str.data(), str.size());
}

string OmahaHashCalculator::OmahaHashOfData(const vector<char>& data) {
  return OmahaHashOfBytes(&data[0], data.size());
}

string OmahaHashCalculator::GetContext() const {
  return string(reinterpret_cast<const char*>(&ctx_), sizeof(ctx_));
}

bool OmahaHashCalculator::SetContext(const std::string& context) {
  TEST_AND_RETURN_FALSE(context.size() == sizeof(ctx_));
  memcpy(&ctx_, context.data(), sizeof(ctx_));
  return true;
}

}  // namespace chromeos_update_engine
