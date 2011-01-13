// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_SIGNER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_SIGNER_H__

#include <string>
#include <vector>

#include <base/basictypes.h>

// This class encapsulates methods used for payload signing and signature
// verification. See update_metadata.proto for more info.

namespace chromeos_update_engine {

extern const uint32_t kSignatureMessageVersion;

class PayloadSigner {
 public:
  // Given a raw |hash| and a private key in |private_key_path| calculates the
  // raw signature in |out_signature|. Returns true on success, false otherwise.
  static bool SignHash(const std::vector<char>& hash,
                       const std::string& private_key_path,
                       std::vector<char>* out_signature);

  // Given an unsigned payload in |unsigned_payload_path| and a private key in
  // |private_key_path|, calculates the signature blob into
  // |out_signature_blob|. Note that the payload must already have an updated
  // manifest that includes the dummy signature op. Returns true on success,
  // false otherwise.
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


  // Given an unsigned payload in |payload_path| (with no dummy signature op)
  // and the raw |signature_size| calculates the raw hash that needs to be
  // signed in |out_hash_data|. Returns true on success, false otherwise.
  static bool HashPayloadForSigning(const std::string& payload_path,
                                    int signature_size,
                                    std::vector<char>* out_hash_data);

  // Given an unsigned payload in |payload_path| (with no dummy signature op)
  // and the raw |signature| updates the payload to include the signature thus
  // turning it into a signed payload. The new payload is stored in
  // |signed_payload_path|. |payload_path| and |signed_payload_path| can point
  // to the same file. Returns true on success, false otherwise.
  static bool AddSignatureToPayload(const std::string& payload_path,
                                    const std::vector<char>& signature,
                                    const std::string& signed_payload_path);

 private:
  // This should never be constructed
  DISALLOW_IMPLICIT_CONSTRUCTORS(PayloadSigner);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_SIGNER_H__
