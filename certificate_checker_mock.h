// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UPDATE_ENGINE_CERTIFICATE_CHECKER_MOCK_H_
#define CHROMEOS_UPDATE_ENGINE_CERTIFICATE_CHECKER_MOCK_H_

#include <gmock/gmock.h>
#include <openssl/ssl.h>

#include "update_engine/certificate_checker.h"

namespace chromeos_update_engine {

class OpenSSLWrapperMock : public OpenSSLWrapper {
 public:
  MOCK_CONST_METHOD4(GetCertificateDigest,
                     bool(X509_STORE_CTX* x509_ctx,
                          int* out_depth,
                          unsigned int* out_digest_length,
                          unsigned char* out_digest));
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_UPDATE_ENGINE_CERTIFICATE_CHECKER_MOCK_H_
