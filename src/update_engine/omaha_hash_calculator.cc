// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/omaha_hash_calculator.h"

#include <fcntl.h>

#include <glog/logging.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>

#include "files/eintr_wrapper.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

// Helper class to free a BIO structure when a method goes out of scope.
class ScopedBioHandle {
 public:
  explicit ScopedBioHandle(BIO* bio) : bio_(bio) {}
  ~ScopedBioHandle() {
    FreeCurrentBio();
  }

  void set_bio(BIO* bio) {
    if (bio_ != bio) {
      // Free the current bio, but only if the caller is not trying to set
      // the same bio object again, so that the operation can be idempotent.
      FreeCurrentBio();
    }
    bio_ = bio;
  }

  BIO* bio() {
    return bio_;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedBioHandle);
  BIO* bio_;

  void FreeCurrentBio() {
    if (bio_) {
      BIO_free_all(bio_);
      bio_ = NULL;
    }
  }
};

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
    LOG(ERROR) << "BIO_new(BIO_f_base64()) failed";
  BIO *bmem = BIO_new(BIO_s_mem());
  if (!bmem)
    LOG(ERROR) << "BIO_new(BIO_s_mem()) failed";
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

bool OmahaHashCalculator::Base64Decode(const string& raw_in,
                                       vector<char>* out) {
  out->clear();

  ScopedBioHandle b64(BIO_new(BIO_f_base64()));
  if (!b64.bio()) {
    LOG(ERROR) << "Unable to create BIO object to decode base64 hash";
    return false;
  }

  // Canonicalize the raw input to get rid of all newlines in the string
  // and set the NO_NL flag so that BIO_read decodes properly. Otherwise
  // BIO_read would just return 0 without decode anything.
  string in;
  for (size_t i = 0; i < raw_in.size(); i++)
    if (raw_in[i] != '\n')
      in.push_back(raw_in[i]);

  BIO_set_flags(b64.bio(), BIO_FLAGS_BASE64_NO_NL);

  BIO *bmem = BIO_new_mem_buf(const_cast<char*>(in.c_str()), in.size());
  if (!bmem) {
    LOG(ERROR) << "Unable to get BIO buffer to decode base64 hash";
    return false;
  }

  b64.set_bio(BIO_push(b64.bio(), bmem));

  const int kOutBufferSize = 1024;
  char out_buffer[kOutBufferSize];
  int num_bytes_read = 1; // any non-zero value is fine to enter the loop.
  while (num_bytes_read > 0) {
    num_bytes_read = BIO_read(b64.bio(), &out_buffer, kOutBufferSize);
    for (int i = 0; i < num_bytes_read; i++)
      out->push_back(out_buffer[i]);
  }

  LOG(INFO) << "Decoded " << out->size()
            << " bytes from " << in.size() << " base64-encoded bytes";
  return true;
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
  return Base64Encode(&raw_hash_[0], raw_hash_.size(), &hash_);
}

bool OmahaHashCalculator::RawHashOfBytes(const char* data,
                                         size_t length,
                                         vector<char>* out_hash) {
  OmahaHashCalculator calc;
  TEST_AND_RETURN_FALSE(calc.Update(data, length));
  TEST_AND_RETURN_FALSE(calc.Finalize());
  *out_hash = calc.raw_hash();
  return true;
}

bool OmahaHashCalculator::RawHashOfData(const vector<char>& data,
                                        vector<char>* out_hash) {
  return RawHashOfBytes(data.data(), data.size(), out_hash);
}

off_t OmahaHashCalculator::RawHashOfFile(const string& name, off_t length,
                                         vector<char>* out_hash) {
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
