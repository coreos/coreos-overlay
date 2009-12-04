// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/gzip.h"
#include <stdlib.h>
#include <algorithm>
#include <zlib.h>
#include "chromeos/obsolete_logging.h"
#include "update_engine/utils.h"

using std::max;
using std::string;
using std::vector;

namespace chromeos_update_engine {

bool GzipDecompressData(const char* const in, const size_t in_size,
                        char** out, size_t* out_size) {
  if (in_size == 0) {
    // malloc(0) may legally return NULL, so do malloc(1)
    *out = reinterpret_cast<char*>(malloc(1));
    *out_size = 0;
    return true;
  }
  TEST_AND_RETURN_FALSE(out);
  TEST_AND_RETURN_FALSE(out_size);
  z_stream stream;
  memset(&stream, 0, sizeof(stream));
  TEST_AND_RETURN_FALSE(inflateInit2(&stream, 16 + MAX_WBITS) == Z_OK);

  // guess that output will be roughly double the input size
  *out_size = in_size * 2;
  *out = reinterpret_cast<char*>(malloc(*out_size));
  TEST_AND_RETURN_FALSE(*out);

  // TODO(adlr): ensure that this const_cast is safe.
  stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(in));
  stream.avail_in = in_size;
  stream.next_out = reinterpret_cast<Bytef*>(*out);
  stream.avail_out = *out_size;
  for (;;) {
    int rc = inflate(&stream, Z_FINISH);
    switch (rc) {
      case Z_STREAM_END: {
        *out_size = reinterpret_cast<char*>(stream.next_out) - (*out);
        TEST_AND_RETURN_FALSE(inflateEnd(&stream) == Z_OK);
        return true;
      }
      case Z_OK:  // fall through
      case Z_BUF_ERROR: {
        // allocate more space
        ptrdiff_t out_length =
            reinterpret_cast<char*>(stream.next_out) - (*out);
        *out_size *= 2;
        char* new_out = reinterpret_cast<char*>(realloc(*out, *out_size));
        if (!new_out) {
          free(*out);
          return false;
        }
        *out = new_out;
        stream.next_out = reinterpret_cast<Bytef*>((*out) + out_length);
        stream.avail_out = (*out_size) - out_length;
        break;
      }
      default:
        LOG(INFO) << "Unknown inflate() return value: " << rc;
        if (stream.msg)
          LOG(INFO) << " message: " << stream.msg;
        free(*out);
        return false;
    }
  }
}

bool GzipCompressData(const char* const in, const size_t in_size,
                      char** out, size_t* out_size) {
  if (in_size == 0) {
    // malloc(0) may legally return NULL, so do malloc(1)
    *out = reinterpret_cast<char*>(malloc(1));
    *out_size = 0;
    return true;
  }
  TEST_AND_RETURN_FALSE(out);
  TEST_AND_RETURN_FALSE(out_size);
  z_stream stream;
  memset(&stream, 0, sizeof(stream));
  TEST_AND_RETURN_FALSE(deflateInit2(&stream,
                                      Z_BEST_COMPRESSION,
                                      Z_DEFLATED,
                                      16 + MAX_WBITS,
                                      9,  // most memory used/best compression
                                      Z_DEFAULT_STRATEGY) == Z_OK);

  // guess that output will be roughly half the input size
  *out_size = max(1U, in_size / 2);
  *out = reinterpret_cast<char*>(malloc(*out_size));
  TEST_AND_RETURN_FALSE(*out);

  // TODO(adlr): ensure that this const_cast is safe.
  stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(in));
  stream.avail_in = in_size;
  stream.next_out = reinterpret_cast<Bytef*>(*out);
  stream.avail_out = *out_size;
  for (;;) {
    int rc = deflate(&stream, Z_FINISH);
    switch (rc) {
      case Z_STREAM_END: {
        *out_size = reinterpret_cast<char*>(stream.next_out) - (*out);
        TEST_AND_RETURN_FALSE(deflateEnd(&stream) == Z_OK);
        return true;
      }
      case Z_OK:  // fall through
      case Z_BUF_ERROR: {
        // allocate more space
        ptrdiff_t out_length =
            reinterpret_cast<char*>(stream.next_out) - (*out);
        *out_size *= 2;
        char* new_out = reinterpret_cast<char*>(realloc(*out, *out_size));
        if (!new_out) {
          free(*out);
          return false;
        }
        *out = new_out;
        stream.next_out = reinterpret_cast<Bytef*>((*out) + out_length);
        stream.avail_out = (*out_size) - out_length;
        break;
      }
      default:
        LOG(INFO) << "Unknown defalate() return value: " << rc;
        if (stream.msg)
          LOG(INFO) << " message: " << stream.msg;
        free(*out);
        return false;
    }
  }
}

bool GzipDecompress(const std::vector<char>& in, std::vector<char>* out) {
  TEST_AND_RETURN_FALSE(out);
  char* out_buf;
  size_t out_size;
  TEST_AND_RETURN_FALSE(GzipDecompressData(&in[0], in.size(),
                                            &out_buf, &out_size));
  out->insert(out->end(), out_buf, out_buf + out_size);
  free(out_buf);
  return true;
}

bool GzipCompress(const std::vector<char>& in, std::vector<char>* out) {
  TEST_AND_RETURN_FALSE(out);
  char* out_buf;
  size_t out_size;
  TEST_AND_RETURN_FALSE(GzipCompressData(&in[0], in.size(),
                                          &out_buf, &out_size));
  out->insert(out->end(), out_buf, out_buf + out_size);
  free(out_buf);
  return true;
}

bool GzipCompressString(const std::string& str,
                        std::vector<char>* out) {
  TEST_AND_RETURN_FALSE(out);
  char* out_buf;
  size_t out_size;
  TEST_AND_RETURN_FALSE(GzipCompressData(str.data(), str.size(),
                                          &out_buf, &out_size));
  out->insert(out->end(), out_buf, out_buf + out_size);
  free(out_buf);
  return true;
}

bool GzipDecompressString(const std::string& str,
                          std::vector<char>* out) {
  TEST_AND_RETURN_FALSE(out);
  char* out_buf;
  size_t out_size;
  TEST_AND_RETURN_FALSE(GzipDecompressData(str.data(), str.size(),
                                            &out_buf, &out_size));
  out->insert(out->end(), out_buf, out_buf + out_size);
  free(out_buf);
  return true;
}

} // namespace chromeos_update_engine
