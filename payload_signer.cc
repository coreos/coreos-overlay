// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_signer.h"

#include <base/logging.h>
#include <base/string_split.h>
#include <base/string_util.h>
#include <openssl/pem.h>

#include "update_engine/delta_diff_generator.h"
#include "update_engine/delta_performer.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/subprocess.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

const uint32_t kSignatureMessageOriginalVersion = 1;
const uint32_t kSignatureMessageCurrentVersion = 1;

namespace {

const char kRSA2048SHA256Padding[] = {
  0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x30, 0x31, 0x30,
  0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
  0x00, 0x04, 0x20
};

// Given raw |signatures|, packs them into a protobuf and serializes it into a
// binary blob. Returns true on success, false otherwise.
bool ConvertSignatureToProtobufBlob(const vector<vector<char> >& signatures,
                                    vector<char>* out_signature_blob) {
  // Pack it into a protobuf
  Signatures out_message;
  uint32_t version = kSignatureMessageOriginalVersion;
  LOG_IF(WARNING, kSignatureMessageCurrentVersion -
         kSignatureMessageOriginalVersion + 1 < signatures.size())
      << "You may want to support clients in the rage ["
      << kSignatureMessageOriginalVersion << ", "
      << kSignatureMessageCurrentVersion << "] inclusive, but you only "
      << "provided " << signatures.size() << " signatures.";
  for (vector<vector<char> >::const_iterator it = signatures.begin(),
           e = signatures.end(); it != e; ++it) {
    const vector<char>& signature = *it;
    Signatures_Signature* sig_message = out_message.add_signatures();
    sig_message->set_version(version++);
    sig_message->set_data(signature.data(), signature.size());
  }

  // Serialize protobuf
  string serialized;
  TEST_AND_RETURN_FALSE(out_message.AppendToString(&serialized));
  out_signature_blob->insert(out_signature_blob->end(),
                             serialized.begin(),
                             serialized.end());
  LOG(INFO) << "Signature blob size: " << out_signature_blob->size();
  return true;
}

bool LoadPayload(const string& payload_path,
                 vector<char>* out_payload,
                 DeltaArchiveManifest* out_manifest,
                 uint64_t* out_metadata_size) {
  vector<char> payload;
  // Loads the payload and parses the manifest.
  TEST_AND_RETURN_FALSE(utils::ReadFile(payload_path, &payload));
  LOG(INFO) << "Payload size: " << payload.size();
  TEST_AND_RETURN_FALSE(DeltaPerformer::ParsePayloadMetadata(
      payload, out_manifest, out_metadata_size) ==
                        DeltaPerformer::kMetadataParseSuccess);
  LOG(INFO) << "Metadata size: " << *out_metadata_size;
  out_payload->swap(payload);
  return true;
}

// Given an unsigned payload under |payload_path| and the |signature_blob_size|
// generates an updated payload that includes a dummy signature op in its
// manifest. Returns true on success, false otherwise.
bool AddSignatureOpToPayload(const string& payload_path,
                             int signature_blob_size,
                             vector<char>* out_payload) {
  const int kProtobufOffset = 20;
  const int kProtobufSizeOffset = 12;

  // Loads the payload.
  vector<char> payload;
  DeltaArchiveManifest manifest;
  uint64_t metadata_size;
  TEST_AND_RETURN_FALSE(LoadPayload(
      payload_path, &payload, &manifest, &metadata_size));
  TEST_AND_RETURN_FALSE(!manifest.has_signatures_offset() &&
                        !manifest.has_signatures_size());

  // Updates the manifest to include the signature operation.
  DeltaDiffGenerator::AddSignatureOp(payload.size() - metadata_size,
                                     signature_blob_size,
                                     &manifest);

  // Updates the payload to include the new manifest.
  string serialized_manifest;
  TEST_AND_RETURN_FALSE(manifest.AppendToString(&serialized_manifest));
  LOG(INFO) << "Updated protobuf size: " << serialized_manifest.size();
  payload.erase(payload.begin() + kProtobufOffset,
                payload.begin() + metadata_size);
  payload.insert(payload.begin() + kProtobufOffset,
                 serialized_manifest.begin(),
                 serialized_manifest.end());

  // Updates the protobuf size.
  uint64_t size_be = htobe64(serialized_manifest.size());
  memcpy(&payload[kProtobufSizeOffset], &size_be, sizeof(size_be));
  LOG(INFO) << "Updated payload size: " << payload.size();
  out_payload->swap(payload);
  return true;
}
}  // namespace {}

bool PayloadSigner::SignHash(const vector<char>& hash,
                             const string& private_key_path,
                             vector<char>* out_signature) {
  string sig_path;
  TEST_AND_RETURN_FALSE(
      utils::MakeTempFile("/tmp/signature.XXXXXX", &sig_path, NULL));
  ScopedPathUnlinker sig_path_unlinker(sig_path);

  string hash_path;
  TEST_AND_RETURN_FALSE(
      utils::MakeTempFile("/tmp/hash.XXXXXX", &hash_path, NULL));
  ScopedPathUnlinker hash_path_unlinker(hash_path);
  // We expect unpadded SHA256 hash coming in
  TEST_AND_RETURN_FALSE(hash.size() == 32);
  vector<char> padded_hash(hash);
  PadRSA2048SHA256Hash(&padded_hash);
  TEST_AND_RETURN_FALSE(utils::WriteFile(hash_path.c_str(),
                                         padded_hash.data(),
                                         padded_hash.size()));

  // This runs on the server, so it's okay to cop out and call openssl
  // executable rather than properly use the library
  vector<string> cmd;
  base::SplitString("/usr/bin/openssl rsautl -raw -sign -inkey x -in x -out x",
                    ' ',
                    &cmd);
  cmd[cmd.size() - 5] = private_key_path;
  cmd[cmd.size() - 3] = hash_path;
  cmd[cmd.size() - 1] = sig_path;

  int return_code = 0;
  TEST_AND_RETURN_FALSE(Subprocess::SynchronousExec(cmd, &return_code, NULL));
  TEST_AND_RETURN_FALSE(return_code == 0);

  vector<char> signature;
  TEST_AND_RETURN_FALSE(utils::ReadFile(sig_path, &signature));
  out_signature->swap(signature);
  return true;
}

bool PayloadSigner::SignPayload(const string& unsigned_payload_path,
                                const vector<string>& private_key_paths,
                                vector<char>* out_signature_blob) {
  vector<char> hash_data;
  TEST_AND_RETURN_FALSE(OmahaHashCalculator::RawHashOfFile(
      unsigned_payload_path, -1, &hash_data) ==
                        utils::FileSize(unsigned_payload_path));

  vector<vector<char> > signatures;
  for (vector<string>::const_iterator it = private_key_paths.begin(),
           e = private_key_paths.end(); it != e; ++it) {
    vector<char> signature;
    TEST_AND_RETURN_FALSE(SignHash(hash_data, *it, &signature));
    signatures.push_back(signature);
  }
  TEST_AND_RETURN_FALSE(ConvertSignatureToProtobufBlob(signatures,
                                                       out_signature_blob));
  return true;
}

bool PayloadSigner::SignatureBlobLength(const vector<string>& private_key_paths,
                                        uint64_t* out_length) {
  DCHECK(out_length);

  string x_path;
  TEST_AND_RETURN_FALSE(
      utils::MakeTempFile("/tmp/signed_data.XXXXXX", &x_path, NULL));
  ScopedPathUnlinker x_path_unlinker(x_path);
  TEST_AND_RETURN_FALSE(utils::WriteFile(x_path.c_str(), "x", 1));

  vector<char> sig_blob;
  TEST_AND_RETURN_FALSE(PayloadSigner::SignPayload(x_path,
                                                   private_key_paths,
                                                   &sig_blob));
  *out_length = sig_blob.size();
  return true;
}

bool PayloadSigner::VerifySignature(const std::vector<char>& signature_blob,
                                    const std::string& public_key_path,
                                    std::vector<char>* out_hash_data) {
  return VerifySignatureVersion(signature_blob, public_key_path,
                                kSignatureMessageCurrentVersion, out_hash_data);
}

bool PayloadSigner::VerifySignatureVersion(
    const std::vector<char>& signature_blob,
    const std::string& public_key_path,
    uint32_t client_version,
    std::vector<char>* out_hash_data) {
  TEST_AND_RETURN_FALSE(!public_key_path.empty());

  Signatures signatures;
  TEST_AND_RETURN_FALSE(signatures.ParseFromArray(&signature_blob[0],
                                                  signature_blob.size()));

  // Finds a signature that matches the current version.
  int sig_index = 0;
  for (; sig_index < signatures.signatures_size(); sig_index++) {
    const Signatures_Signature& signature = signatures.signatures(sig_index);
    if (signature.has_version() &&
        signature.version() == client_version) {
      break;
    }
  }
  TEST_AND_RETURN_FALSE(sig_index < signatures.signatures_size());

  const Signatures_Signature& signature = signatures.signatures(sig_index);
  const string& sig_data = signature.data();

  // The code below executes the equivalent of:
  //
  // openssl rsautl -verify -pubin -inkey |public_key_path|
  //   -in |sig_data| -out |out_hash_data|

  // Loads the public key.
  FILE* fpubkey = fopen(public_key_path.c_str(), "rb");
  TEST_AND_RETURN_FALSE(fpubkey != NULL);
  char dummy_password[] = { ' ', 0 };  // Ensure no password is read from stdin.
  RSA* rsa = PEM_read_RSA_PUBKEY(fpubkey, NULL, NULL, dummy_password);
  fclose(fpubkey);
  TEST_AND_RETURN_FALSE(rsa != NULL);
  unsigned int keysize = RSA_size(rsa);
  if (sig_data.size() > 2 * keysize) {
    LOG(ERROR) << "Signature size is too big for public key size.";
    RSA_free(rsa);
    return false;
  }

  // Decrypts the signature.
  vector<char> hash_data(keysize);
  int decrypt_size = RSA_public_decrypt(
      sig_data.size(),
      reinterpret_cast<const unsigned char*>(sig_data.data()),
      reinterpret_cast<unsigned char*>(hash_data.data()),
      rsa,
      RSA_NO_PADDING);
  RSA_free(rsa);
  TEST_AND_RETURN_FALSE(decrypt_size > 0 &&
                        decrypt_size <= static_cast<int>(hash_data.size()));
  hash_data.resize(decrypt_size);
  out_hash_data->swap(hash_data);
  return true;
}

bool PayloadSigner::VerifySignedPayload(const std::string& payload_path,
                                        const std::string& public_key_path,
                                        uint32_t client_key_check_version) {
  vector<char> payload;
  DeltaArchiveManifest manifest;
  uint64_t metadata_size;
  TEST_AND_RETURN_FALSE(LoadPayload(
      payload_path, &payload, &manifest, &metadata_size));
  TEST_AND_RETURN_FALSE(manifest.has_signatures_offset() &&
                        manifest.has_signatures_size());
  CHECK_EQ(payload.size(),
           metadata_size + manifest.signatures_offset() +
           manifest.signatures_size());
  vector<char> signature_blob(
      payload.begin() + metadata_size + manifest.signatures_offset(),
      payload.end());
  vector<char> signed_hash;
  TEST_AND_RETURN_FALSE(VerifySignatureVersion(
      signature_blob, public_key_path, client_key_check_version, &signed_hash));
  TEST_AND_RETURN_FALSE(!signed_hash.empty());
  vector<char> hash;
  TEST_AND_RETURN_FALSE(OmahaHashCalculator::RawHashOfBytes(
      payload.data(), metadata_size + manifest.signatures_offset(), &hash));
  PadRSA2048SHA256Hash(&hash);
  TEST_AND_RETURN_FALSE(hash == signed_hash);
  return true;
}

bool PayloadSigner::HashPayloadForSigning(const std::string& payload_path,
                                          const vector<int>& signature_sizes,
                                          vector<char>* out_hash_data) {
  // TODO(petkov): Reduce memory usage -- the payload is manipulated in memory.

  // Loads the payload and adds the signature op to it.
  vector<vector<char> > signatures;
  for (vector<int>::const_iterator it = signature_sizes.begin(),
           e = signature_sizes.end(); it != e; ++it) {
    vector<char> signature(*it, 0);
    signatures.push_back(signature);
  }
  vector<char> signature_blob;
  TEST_AND_RETURN_FALSE(ConvertSignatureToProtobufBlob(signatures,
                                                       &signature_blob));
  vector<char> payload;
  TEST_AND_RETURN_FALSE(AddSignatureOpToPayload(payload_path,
                                                signature_blob.size(),
                                                &payload));
  // Calculates the hash on the updated payload. Note that the payload includes
  // the signature op but doesn't include the signature blob at the end.
  TEST_AND_RETURN_FALSE(OmahaHashCalculator::RawHashOfData(payload,
                                                           out_hash_data));
  return true;
}

bool PayloadSigner::AddSignatureToPayload(
    const string& payload_path,
    const vector<vector<char> >& signatures,
    const string& signed_payload_path) {
  // TODO(petkov): Reduce memory usage -- the payload is manipulated in memory.

  // Loads the payload and adds the signature op to it.
  vector<char> signature_blob;
  TEST_AND_RETURN_FALSE(ConvertSignatureToProtobufBlob(signatures,
                                                       &signature_blob));
  vector<char> payload;
  TEST_AND_RETURN_FALSE(AddSignatureOpToPayload(payload_path,
                                                signature_blob.size(),
                                                &payload));
  // Appends the signature blob to the end of the payload and writes the new
  // payload.
  payload.insert(payload.end(), signature_blob.begin(), signature_blob.end());
  LOG(INFO) << "Signed payload size: " << payload.size();
  TEST_AND_RETURN_FALSE(utils::WriteFile(signed_payload_path.c_str(),
                                         payload.data(),
                                         payload.size()));
  return true;
}

bool PayloadSigner::PadRSA2048SHA256Hash(std::vector<char>* hash) {
  TEST_AND_RETURN_FALSE(hash->size() == 32);
  hash->insert(hash->begin(),
               kRSA2048SHA256Padding,
               kRSA2048SHA256Padding + sizeof(kRSA2048SHA256Padding));
  TEST_AND_RETURN_FALSE(hash->size() == 256);
  return true;
}

}  // namespace chromeos_update_engine
