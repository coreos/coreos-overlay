// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/certificate_checker.h"

#include <string>

#include <curl/curl.h>
#include <glog/logging.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>

#include "strings/string_number_conversions.h"
#include "strings/string_printf.h"
#include "update_engine/prefs_interface.h"
#include "update_engine/utils.h"

using std::string;
using strings::StringPrintf;

namespace chromeos_update_engine {

namespace {
// This should be in the same order of CertificateChecker::ServerToCheck, with
// the exception of kNone.
static const char* kReportToSendKey[2] =
    {kPrefsCertificateReportToSendUpdate,
     kPrefsCertificateReportToSendDownload};
}  // namespace {}

bool OpenSSLWrapper::GetCertificateDigest(X509_STORE_CTX* x509_ctx,
                                          int* out_depth,
                                          unsigned int* out_digest_length,
                                          unsigned char* out_digest) const {
  TEST_AND_RETURN_FALSE(out_digest);
  X509* certificate = X509_STORE_CTX_get_current_cert(x509_ctx);
  TEST_AND_RETURN_FALSE(certificate);
  int depth = X509_STORE_CTX_get_error_depth(x509_ctx);
  if (out_depth)
    *out_depth = depth;

  unsigned int len;
  const EVP_MD* digest_function = EVP_sha256();
  bool success = X509_digest(certificate, digest_function, out_digest, &len);

  if (success && out_digest_length)
    *out_digest_length = len;
  return success;
}

// static
SystemState* CertificateChecker::system_state_ = NULL;

// static
OpenSSLWrapper* CertificateChecker::openssl_wrapper_ = NULL;

// static
CURLcode CertificateChecker::ProcessSSLContext(CURL* curl_handle,
                                               SSL_CTX* ssl_ctx,
                                               void* ptr) {
  // From here we set the SSL_CTX to another callback, from the openssl library,
  // which will be called after each server certificate is validated. However,
  // since openssl does not allow us to pass our own data pointer to the
  // callback, the certificate check will have to be done statically. Since we
  // need to know which update server we are using in order to check the
  // certificate, we hardcode Chrome OS's two known update servers here, and
  // define a different static callback for each. Since this code should only
  // run in official builds, this should not be a problem. However, if an update
  // server different from the ones listed here is used, the check will not
  // take place.
  ServerToCheck* server_to_check = reinterpret_cast<ServerToCheck*>(ptr);

  // We check which server to check and set the appropriate static callback.
  if (*server_to_check == kUpdate)
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, VerifySSLCallbackUpdateCheck);
  if (*server_to_check == kDownload)
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, VerifySSLCallbackDownload);

  return CURLE_OK;
}

// static
int CertificateChecker::VerifySSLCallbackUpdateCheck(int preverify_ok,
                                                     X509_STORE_CTX* x509_ctx) {
  return CertificateChecker::CheckCertificateChange(
      kUpdate, preverify_ok, x509_ctx) ? 1 : 0;
}

// static
int CertificateChecker::VerifySSLCallbackDownload(int preverify_ok,
                                                  X509_STORE_CTX* x509_ctx) {
  return CertificateChecker::CheckCertificateChange(
      kDownload, preverify_ok, x509_ctx) ? 1 : 0;
}

// static
bool CertificateChecker::CheckCertificateChange(
    ServerToCheck server_to_check, int preverify_ok,
    X509_STORE_CTX* x509_ctx) {
  static const char kUMAActionCertChanged[] =
      "Updater.ServerCertificateChanged";
  static const char kUMAActionCertFailed[] = "Updater.ServerCertificateFailed";
  TEST_AND_RETURN_FALSE(system_state_ != NULL);
  TEST_AND_RETURN_FALSE(system_state_->prefs() != NULL);
  TEST_AND_RETURN_FALSE(server_to_check != kNone);

  // If pre-verification failed, we are not interested in the current
  // certificate. We store a report to UMA and just propagate the fail result.
  if (!preverify_ok) {
    LOG_IF(WARNING, !system_state_->prefs()->SetString(
        kReportToSendKey[server_to_check], kUMAActionCertFailed))
        << "Failed to store UMA report on a failure to validate "
        << "certificate from update server.";
    return false;
  }

  int depth;
  unsigned int digest_length;
  unsigned char digest[EVP_MAX_MD_SIZE];

  if (!openssl_wrapper_->GetCertificateDigest(x509_ctx,
                                              &depth,
                                              &digest_length,
                                              digest)) {
    LOG(WARNING) << "Failed to generate digest of X509 certificate "
                 << "from update server.";
    return true;
  }

  // We convert the raw bytes of the digest to an hex string, for storage in
  // prefs.
  string digest_string = strings::HexEncode(digest, digest_length);

  string storage_key = StringPrintf("%s-%d-%d",
                                    kPrefsUpdateServerCertificate,
                                    server_to_check,
                                    depth);
  string stored_digest;
  // If there's no stored certificate, we just store the current one and return.
  if (!system_state_->prefs()->GetString(storage_key, &stored_digest)) {
    LOG_IF(WARNING, !system_state_->prefs()->SetString(storage_key,
                                                       digest_string))
        << "Failed to store server certificate on storage key " << storage_key;
    return true;
  }

  // Certificate changed, we store a report to UMA and store the most recent
  // certificate.
  if (stored_digest != digest_string) {
    LOG_IF(WARNING, !system_state_->prefs()->SetString(
        kReportToSendKey[server_to_check], kUMAActionCertChanged))
        << "Failed to store UMA report on a change on the "
        << "certificate from update server.";
    LOG_IF(WARNING, !system_state_->prefs()->SetString(storage_key,
                                                       digest_string))
        << "Failed to store server certificate on storage key " << storage_key;
  }

  // Since we don't perform actual SSL verification, we return success.
  return true;
}

// static
void CertificateChecker::FlushReport() {
  // This check shouldn't be needed, but it is useful for testing.
  TEST_AND_RETURN(system_state_);
  TEST_AND_RETURN(system_state_->prefs());

  // We flush reports for both servers.
  for (size_t i = 0; i < arraysize(kReportToSendKey); i++) {
    string report_to_send;
    if (system_state_->prefs()->GetString(kReportToSendKey[i], &report_to_send)
        && !report_to_send.empty()) {
      // There is a report to be sent. We send it and erase it.
      LOG(INFO) << "Found report #" << i << ". Sending it";
      LOG(WARNING)
          << "Failed to send server certificate report to UMA: "
          << report_to_send;
      LOG_IF(WARNING, !system_state_->prefs()->Delete(kReportToSendKey[i]))
          << "Failed to erase server certificate report to be sent to UMA";
    }
  }
}

}  // namespace chromeos_update_engine
