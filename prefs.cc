// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/prefs.h"

#include <base/file_util.h>
#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>

#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

const char kPrefsDeltaUpdateFailures[] = "delta-update-failures";
const char kPrefsLastActivePingDay[] = "last-active-ping-day";
const char kPrefsLastRollCallPingDay[] = "last-roll-call-ping-day";
const char kPrefsManifestMetadataSize[] = "manifest-metadata-size";
const char kPrefsUpdateCheckResponseHash[] = "update-check-response-hash";
const char kPrefsUpdateStateNextDataOffset[] = "update-state-next-data-offset";
const char kPrefsUpdateStateNextOperation[] = "update-state-next-operation";
const char kPrefsUpdateStateSignedSHA256Context[] =
    "update-state-signed-sha-256-context";

bool Prefs::Init(const FilePath& prefs_dir) {
  prefs_dir_ = prefs_dir;
  return true;
}

bool Prefs::GetString(const string& key, string* value) {
  FilePath filename;
  TEST_AND_RETURN_FALSE(GetFileNameForKey(key, &filename));
  TEST_AND_RETURN_FALSE(file_util::ReadFileToString(filename, value));
  return true;
}

bool Prefs::SetString(const std::string& key, const std::string& value) {
  FilePath filename;
  TEST_AND_RETURN_FALSE(GetFileNameForKey(key, &filename));
  TEST_AND_RETURN_FALSE(file_util::CreateDirectory(filename.DirName()));
  TEST_AND_RETURN_FALSE(
      file_util::WriteFile(filename, value.data(), value.size()) ==
      static_cast<int>(value.size()));
  return true;
}

bool Prefs::GetInt64(const string& key, int64_t* value) {
  string str_value;
  TEST_AND_RETURN_FALSE(GetString(key, &str_value));
  TrimWhitespaceASCII(str_value, TRIM_ALL, &str_value);
  TEST_AND_RETURN_FALSE(base::StringToInt64(str_value, value));
  return true;
}

bool Prefs::SetInt64(const string& key, const int64_t value) {
  return SetString(key, base::Int64ToString(value));
}

bool Prefs::GetFileNameForKey(const std::string& key, FilePath* filename) {
  // Allows only non-empty keys containing [A-Za-z0-9_-].
  TEST_AND_RETURN_FALSE(!key.empty());
  for (size_t i = 0; i < key.size(); ++i) {
    char c = key.at(i);
    TEST_AND_RETURN_FALSE(IsAsciiAlpha(c) || IsAsciiDigit(c) ||
                          c == '_' || c == '-');
  }
  *filename = prefs_dir_.Append(key);
  return true;
}

}  // namespace chromeos_update_engine
