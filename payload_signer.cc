// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/payload_signer.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/subprocess.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

const uint32_t kSignatureMessageVersion = 1;

bool PayloadSigner::SignPayload(const string& unsigned_payload_path,
                                const string& private_key_path,
                                vector<char>* out_signature_blob) {
  string sig_path;
  TEST_AND_RETURN_FALSE(
      utils::MakeTempFile("/tmp/signature.XXXXXX", &sig_path, NULL));
  ScopedPathUnlinker sig_path_unlinker(sig_path);

  string hash_path;
  TEST_AND_RETURN_FALSE(
      utils::MakeTempFile("/tmp/hash.XXXXXX", &hash_path, NULL));
  ScopedPathUnlinker hash_path_unlinker(hash_path);
  
  vector<char> hash_data;
  {
    vector<char> payload;
    // TODO(adlr): Read file in chunks. Not urgent as this runs on the server.
    TEST_AND_RETURN_FALSE(utils::ReadFile(unsigned_payload_path, &payload));
    TEST_AND_RETURN_FALSE(OmahaHashCalculator::RawHashOfData(payload,
                                                             &hash_data));
  }
  TEST_AND_RETURN_FALSE(utils::WriteFile(hash_path.c_str(),
                                         &hash_data[0],
                                         hash_data.size()));
  
  // This runs on the server, so it's okay to cop out and call openssl
  // executable rather than properly use the library
  vector<string> cmd;
  SplitString("/usr/bin/openssl rsautl -pkcs -sign -inkey x -in x -out x",
              ' ',
              &cmd);
  cmd[cmd.size() - 5] = private_key_path;
  cmd[cmd.size() - 3] = hash_path;
  cmd[cmd.size() - 1] = sig_path;
  
  int return_code = 0;
  TEST_AND_RETURN_FALSE(Subprocess::SynchronousExec(cmd, &return_code));
  TEST_AND_RETURN_FALSE(return_code == 0);
  
  vector<char> signature;
  TEST_AND_RETURN_FALSE(utils::ReadFile(sig_path, &signature));
  
  // Pack it into a protobuf
  Signatures out_message;
  Signatures_Signature* sig_message = out_message.add_signatures();
  sig_message->set_version(kSignatureMessageVersion);
  sig_message->set_data(signature.data(), signature.size());
  
  // Serialize protobuf
  string serialized;
  TEST_AND_RETURN_FALSE(out_message.AppendToString(&serialized));
  out_signature_blob->insert(out_signature_blob->end(),
                             serialized.begin(),
                             serialized.end());
  return true;
}

bool PayloadSigner::SignatureBlobLength(
    const string& private_key_path,
    uint64_t* out_length) {
  DCHECK(out_length);
  
  string x_path;
  TEST_AND_RETURN_FALSE(
      utils::MakeTempFile("/tmp/signed_data.XXXXXX", &x_path, NULL));
  ScopedPathUnlinker x_path_unlinker(x_path);
  TEST_AND_RETURN_FALSE(utils::WriteFile(x_path.c_str(), "x", 1));

  vector<char> sig_blob;
  TEST_AND_RETURN_FALSE(PayloadSigner::SignPayload(x_path,
                                                   private_key_path,
                                                   &sig_blob));
  *out_length = sig_blob.size();
  return true;
}

}  // namespace chromeos_update_engine
