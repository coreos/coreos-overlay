// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_TEST_UTILS_H__
#define UPDATE_ENGINE_TEST_UTILS_H__

#include <vector>
#include <string>

// These are some handy functions for unittests.

namespace chromeos_update_engine {

// Returns the entire contents of the file at path. If the file doesn't
// exist or error occurrs, an empty vector is returned.
std::vector<char> ReadFile(const std::string& path);

// Writes the data passed to path. The file at path will be overwritten if it
// exists. Returns true on success, false otherwise.
bool WriteFile(const std::string& path, const std::vector<char>& data);

// Returns the size of the file at path. If the file doesn't exist or some
// error occurrs, -1 is returned.
off_t FileSize(const std::string& path);

// Gzip compresses the data passed using the gzip command line program.
// Returns compressed data back.
std::vector<char> GzipCompressData(const std::vector<char>& data);

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_TEST_UTILS_H__