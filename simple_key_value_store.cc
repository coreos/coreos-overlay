// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/simple_key_value_store.h"
#include <map>
#include <string>
#include <vector>
#include "base/string_util.h"

using std::map;
using std::string;
using std::vector;

namespace chromeos_update_engine {
namespace simple_key_value_store {

// Parses a string. 
map<std::string, std::string> ParseString(const string& str) {
  // Split along '\n', then along '='
  std::map<std::string, std::string> ret;
  vector<string> lines;
  SplitStringDontTrim(str, '\n', &lines);
  for (vector<string>::const_iterator it = lines.begin();
       it != lines.end(); ++it) {
    string::size_type pos = it->find('=');
    if (pos == string::npos)
      continue;
    ret[it->substr(0, pos)] = it->substr(pos + 1);
  }
  return ret;
}

string AssembleString(const std::map<string, string>& data) {
  string ret;
  for (std::map<string, string>::const_iterator it = data.begin();
       it != data.end(); ++it) {
    ret += it->first;
    ret += "=";
    ret += it->second;
    ret += "\n";
  }
  return ret;
}

}  // namespace simple_key_value_store
}  // namespace chromeos_update_engine
