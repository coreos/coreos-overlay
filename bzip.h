// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

namespace chromeos_update_engine {

// Bzip2 compresses or decompresses str/in to out.
bool BzipDecompress(const std::vector<char>& in, std::vector<char>* out);
bool BzipCompress(const std::vector<char>& in, std::vector<char>* out);
bool BzipCompressString(const std::string& str, std::vector<char>* out);
bool BzipDecompressString(const std::string& str, std::vector<char>* out);

}  // namespace chromeos_update_engine
