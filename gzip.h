// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

namespace chromeos_update_engine {

// Gzip compresses or decompresses the input to the output.
// Returns true on success. If true, *out will point to a malloc()ed
// buffer, which must be free()d by the caller.
bool GzipCompressData(const char* const in, const size_t in_size,
                      char** out, size_t* out_size);
bool GzipDecompressData(const char* const in, const size_t in_size,
                        char** out, size_t* out_size);

// Helper functions:
bool GzipDecompress(const std::vector<char>& in, std::vector<char>* out);
bool GzipCompress(const std::vector<char>& in, std::vector<char>* out);
bool GzipCompressString(const std::string& str, std::vector<char>* out);
bool GzipDecompressString(const std::string& str, std::vector<char>* out);

}  // namespace chromeos_update_engine {
