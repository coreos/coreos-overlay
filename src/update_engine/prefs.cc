// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/prefs.h"

#include <glog/logging.h>

#include "files/file_util.h"
#include "strings/string_number_conversions.h"
#include "strings/string_split.h"
#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

const char kPrefsCertificateReportToSendDownload[] =
    "certificate-report-to-send-download";
const char kPrefsCertificateReportToSendUpdate[] =
    "certificate-report-to-send-update";
const char kPrefsDeltaUpdateFailures[] = "delta-update-failures";
const char kPrefsManifestMetadataSize[] = "manifest-metadata-size";
const char kPrefsPreviousVersion[] = "previous-version";
const char kPrefsResumedUpdateFailures[] = "resumed-update-failures";
const char kPrefsUpdateCheckResponseHash[] = "update-check-response-hash";
const char kPrefsUpdateServerCertificate[] = "update-server-cert";
const char kPrefsUpdateStateNextDataOffset[] = "update-state-next-data-offset";
const char kPrefsUpdateStateNextOperation[] = "update-state-next-operation";
const char kPrefsUpdateStateSHA256Context[] = "update-state-sha-256-context";
const char kPrefsUpdateStateSignatureBlob[] = "update-state-signature-blob";
const char kPrefsUpdateStateSignedSHA256Context[] =
    "update-state-signed-sha-256-context";

const char kPrefsPayloadAttemptNumber[] = "payload-attempt-number";
const char kPrefsCurrentResponseSignature[] = "current-response-signature";
const char kPrefsCurrentUrlIndex[] = "current-url-index";
const char kPrefsCurrentUrlFailureCount[] = "current-url-failure-count";
const char kPrefsBackoffExpiryTime[] = "backoff-expiry-time";
const char kPrefsAlephVersion[] = "aleph-version";

bool Prefs::Init(const files::FilePath& prefs_dir) {
  prefs_dir_ = prefs_dir;
  return true;
}

bool Prefs::GetString(const string& key, string* value) {
  files::FilePath filename;
  TEST_AND_RETURN_FALSE(GetFileNameForKey(key, &filename));
  if (!files::ReadFileToString(filename, value)) {
    LOG(INFO) << key << " not present in " << prefs_dir_.value();
    return false;
  }
  return true;
}

bool Prefs::SetString(const std::string& key, const std::string& value) {
  files::FilePath filename;
  TEST_AND_RETURN_FALSE(GetFileNameForKey(key, &filename));
  TEST_AND_RETURN_FALSE(files::CreateDirectory(filename.DirName()));
  TEST_AND_RETURN_FALSE(
      files::WriteFile(filename, value.data(), value.size()) ==
      static_cast<int>(value.size()));
  return true;
}

bool Prefs::GetInt64(const string& key, int64_t* value) {
  string str_value;
  if (!GetString(key, &str_value))
    return false;
  str_value = strings::TrimWhitespace(str_value);
  TEST_AND_RETURN_FALSE(strings::StringToInt64(str_value, value));
  return true;
}

bool Prefs::SetInt64(const string& key, const int64_t value) {
  return SetString(key, std::to_string(value));
}

bool Prefs::Exists(const string& key) {
  files::FilePath filename;
  TEST_AND_RETURN_FALSE(GetFileNameForKey(key, &filename));
  return files::PathExists(filename);
}

bool Prefs::Delete(const string& key) {
  files::FilePath filename;
  TEST_AND_RETURN_FALSE(GetFileNameForKey(key, &filename));
  return files::DeleteFile(filename, false);
}

bool Prefs::GetFileNameForKey(const std::string& key, files::FilePath* filename) {
  // Allows only non-empty keys containing [A-Za-z0-9_-].
  TEST_AND_RETURN_FALSE(!key.empty());
  for (size_t i = 0; i < key.size(); ++i) {
    char c = key.at(i);
    TEST_AND_RETURN_FALSE(('A' <= c && c <= 'Z') ||
                          ('a' <= c && c <= 'z') ||
                          ('0' <= c && c <= '9') ||
                          c == '_' || c == '-');
  }
  *filename = prefs_dir_.Append(key);
  return true;
}

}  // namespace chromeos_update_engine
