// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_CERTIFICATE_CHECKER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_CERTIFICATE_CHECKER_H_

#include <string>

#include <curl/curl.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST
#include <openssl/ssl.h>

#include "macros.h"
#include "update_engine/system_state.h"

namespace chromeos_update_engine {

// Wrapper for openssl operations with the certificates.
class OpenSSLWrapper {
 public:
  OpenSSLWrapper() {}
  virtual ~OpenSSLWrapper() {}

  // Takes an openssl X509_STORE_CTX, extracts the corresponding certificate
  // from it and calculates its fingerprint (SHA256 digest). Returns true on
  // success and false otherwise.
  //
  // |x509_ctx| is the pointer to the openssl object that holds the certificate.
  // |out_depth| is the depth of the current certificate, in the certificate
  // chain.
  // |out_digest_length| is the length of the generated digest.
  // |out_digest| is the byte array where the digest itself will be written.
  // It should be big enough to hold a SHA1 digest (e.g. EVP_MAX_MD_SIZE).
  virtual bool GetCertificateDigest(X509_STORE_CTX* x509_ctx,
                                    int* out_depth,
                                    unsigned int* out_digest_length,
                                    unsigned char* out_digest) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(OpenSSLWrapper);
};

// Responsible for checking whether update server certificates change, and
// reporting to UMA when this happens. Since all state information is persisted,
// and openssl forces us to use a static callback with no data pointer, this
// class is entirely static.
class CertificateChecker {
 public:
  // These values are used to generate the keys of files persisted via prefs.
  // This means that changing these will cause loss of information on metrics
  // reporting, during the transition.
  enum ServerToCheck {
    kUpdate = 0,
    kDownload = 1,
    kNone = 2  // This needs to be the last element. Changing its value is ok.
  };

  CertificateChecker() {}
  virtual ~CertificateChecker() {}

  // This callback is called by libcurl just before the initialization of an
  // SSL connection after having processed all other SSL related options. Used
  // to check if server certificates change. |ptr| is expected to be a
  // pointer to a ServerToCheck.
  static CURLcode ProcessSSLContext(CURL* curl_handle, SSL_CTX* ssl_ctx,
                                    void* ptr);

  // Flushes to UMA any certificate-related report that was persisted.
  static void FlushReport();

  // Setters.
  static void set_system_state(SystemState* system_state) {
    system_state_ = system_state;
  }

  static void set_openssl_wrapper(OpenSSLWrapper* openssl_wrapper) {
    openssl_wrapper_ = openssl_wrapper;
  }

 private:
  FRIEND_TEST(CertificateCheckerTest, NewCertificate);
  FRIEND_TEST(CertificateCheckerTest, SameCertificate);
  FRIEND_TEST(CertificateCheckerTest, ChangedCertificate);
  FRIEND_TEST(CertificateCheckerTest, FailedCertificate);
  FRIEND_TEST(CertificateCheckerTest, FlushReport);
  FRIEND_TEST(CertificateCheckerTest, FlushNothingToReport);

  // These callbacks are called by openssl after initial SSL verification. They
  // are used to perform any additional security verification on the connection,
  // but we use them here to get hold of the server certificate, in order to
  // determine if it has changed since the last connection. Since openssl forces
  // us to do this statically, we define two different callbacks for the two
  // different official update servers, and only assign the correspondent one.
  // The assigned callback is then called once per each certificate on the
  // server and returns 1 for success and 0 for failure.
  static int VerifySSLCallbackUpdateCheck(int preverify_ok,
                                          X509_STORE_CTX* x509_ctx);
  static int VerifySSLCallbackDownload(int preverify_ok,
                                       X509_STORE_CTX* x509_ctx);

  // Checks if server certificate for |server_to_check|, stored in |x509_ctx|,
  // has changed since last connection to that same server. This is called by
  // one of the two callbacks defined above. If certificate fails to check or
  // changes, a report is generated and persisted, to be later sent by
  // FlushReport. Returns true on success and false otherwise.
  static bool CheckCertificateChange(ServerToCheck server_to_check,
                                     int preverify_ok,
                                     X509_STORE_CTX* x509_ctx);

  // Global system context.
  static SystemState* system_state_;

  // The wrapper for openssl operations.
  static OpenSSLWrapper* openssl_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(CertificateChecker);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_CERTIFICATE_CHECKER_H_
