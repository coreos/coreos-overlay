// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/gzip.h"
#include <stdlib.h>
#include <algorithm>
#include <bzlib.h>
#include "update_engine/utils.h"

using std::max;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {

// BzipData compresses or decompresses the input to the output.
// Returns true on success.
// Use one of BzipBuffToBuff*ompress as the template parameter to BzipData().
int BzipBuffToBuffDecompress(char* out,
                             size_t* out_length,
                             const char* in,
                             size_t in_length) {
  return BZ2_bzBuffToBuffDecompress(out,
                                    out_length,
                                    const_cast<char*>(in),
                                    in_length,
                                    0,  // Silent verbosity
                                    0);  // Normal algorithm
}

int BzipBuffToBuffCompress(char* out,
                           size_t* out_length,
                           const char* in,
                           size_t in_length) {
  return BZ2_bzBuffToBuffCompress(out,
                                  out_length,
                                  const_cast<char*>(in),
                                  in_length,
                                  9,  // Best compression
                                  0,  // Silent verbosity
                                  0);  // Default work factor
}

template<int F(char* out,
               size_t* out_length,
               const char* in,
               size_t in_length)>
bool BzipData(const char* const in,
              const size_t in_size,
              vector<char>* const out) {
  TEST_AND_RETURN_FALSE(out);
  out->clear();
  if (in_size == 0) {
    return true;
  }
  // Try increasing buffer size until it works
  size_t buf_size = in_size;
  out->resize(buf_size);
  
  for (;;) {
    size_t data_size = buf_size;
    int rc = F(&(*out)[0], &data_size, in, in_size);
    TEST_AND_RETURN_FALSE(rc == BZ_OUTBUFF_FULL || rc == BZ_OK);
    if (rc == BZ_OK) {
      // we're done!
      out->resize(data_size);
      return true;
    }
    
    // Data didn't fit; double the buffer size.
    buf_size *= 2;
    out->resize(buf_size);
  }
}

}  // namespace {}

bool BzipDecompress(const std::vector<char>& in, std::vector<char>* out) {
  return BzipData<BzipBuffToBuffDecompress>(&in[0], in.size(), out);
}

bool BzipCompress(const std::vector<char>& in, std::vector<char>* out) {
  return BzipData<BzipBuffToBuffCompress>(&in[0], in.size(), out);
}

namespace {
template<bool F(const char* const in,
                const size_t in_size,
                vector<char>* const out)>
bool BzipString(const std::string& str,
                std::vector<char>* out) {
  TEST_AND_RETURN_FALSE(out);
  vector<char> temp;
  TEST_AND_RETURN_FALSE(F(str.data(),
                          str.size(),
                          &temp));
  out->clear();
  out->insert(out->end(), temp.begin(), temp.end());
  return true;
}
}  // namespace {}

bool BzipCompressString(const std::string& str,
                        std::vector<char>* out) {
  return BzipString<BzipData<BzipBuffToBuffCompress> >(str, out);
}

bool BzipDecompressString(const std::string& str,
                          std::vector<char>* out) {
  return BzipString<BzipData<BzipBuffToBuffDecompress> >(str, out);
}

} // namespace chromeos_update_engine
