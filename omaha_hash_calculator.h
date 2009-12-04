// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_HASH_CALCULATOR_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_HASH_CALCULATOR_H__

#include "base/basictypes.h"
#include <string>
#include <vector>
#include <openssl/sha.h>

// Omaha uses base64 encoded SHA-1 as the hash. This class provides a simple
// wrapper around OpenSSL providing such a formatted hash of data passed in.
// The methods of this class must be called in a very specific order:
// First the ctor (of course), then 0 or more calls to Update(), then
// Finalize(), then 0 or more calls to hash().

namespace chromeos_update_engine {

class OmahaHashCalculator {
 public:
   OmahaHashCalculator();

  // Update is called with all of the data that should be hashed in order.
  // Update will read |length| bytes of |data|
   void Update(const char* data, size_t length);

  // Call Finalize() when all data has been passed in. This method tells
  // OpenSSl that no more data will come in and base64 encodes the resulting
  // hash.
   void Finalize();

  // Gets the hash. Finalize() must have been called.
  const std::string& hash() const {
    CHECK(!hash_.empty()) << "Call Finalize() first";
    return hash_;
  }

  // Used by tests
  static std::string OmahaHashOfBytes(const void* data, size_t length);
  static std::string OmahaHashOfString(const std::string& str);
  static std::string OmahaHashOfData(const std::vector<char>& data);

 private:
  // If non-empty, the final base64 encoded hash. Will only be set to
  // non-empty when Finalize is called.
  std::string hash_;

  // The hash state used by OpenSSL
  SHA_CTX ctx_;
  DISALLOW_COPY_AND_ASSIGN(OmahaHashCalculator);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_HASH_CALCULATOR_H__