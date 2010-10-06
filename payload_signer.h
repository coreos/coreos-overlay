// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_SIGNER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_SIGNER_H__

#include <string>
#include <vector>
#include "base/basictypes.h"

// This function signs a payload with the OS vendor's private key.
// It takes an update up to the signature blob and returns the signature
// blob, which should be appended. See update_metadata.proto for more info.

namespace chromeos_update_engine {

extern const uint32_t kSignatureMessageVersion;

class PayloadSigner {
 public:
  static bool SignPayload(const std::string& unsigned_payload_path,
                          const std::string& private_key_path,
                          std::vector<char>* out_signature_blob);

  // Returns the length of out_signature_blob that will result in a call
  // to SignPayload with a given private key. Returns true on success.
  static bool SignatureBlobLength(const std::string& private_key_path,
                                  uint64_t* out_length);

  // Returns false if the payload signature can't be verified. Returns true
  // otherwise and sets |out_hash| to the signed payload hash.
  static bool VerifySignature(const std::vector<char>& signature_blob,
                              const std::string& public_key_path,
                              std::vector<char>* out_hash_data);

 private:
  // This should never be constructed
  DISALLOW_IMPLICIT_CONSTRUCTORS(PayloadSigner);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_SIGNER_H__
