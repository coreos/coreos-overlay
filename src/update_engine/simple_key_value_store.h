// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These functions can parse a blob of data that's formatted as a simple
// key value store. Each key/value pair is stored on its own line and
// separated by the first '=' on the line.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_SIMPLE_KEY_VALUE_STORE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_SIMPLE_KEY_VALUE_STORE_H__

#include <map>
#include <string>

namespace chromeos_update_engine {
namespace simple_key_value_store {
  
// Parses a string. 
std::map<std::string, std::string> ParseString(const std::string& str);

std::string AssembleString(const std::map<std::string, std::string>& data);

}  // namespace simple_key_value_store
}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_SIMPLE_KEY_VALUE_STORE_H__
