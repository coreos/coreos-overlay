// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/decompressing_file_writer.h"

namespace chromeos_update_engine {

//  typedef struct z_stream_s {
//      Bytef    *next_in;  /* next input byte */
//      uInt     avail_in;  /* number of bytes available at next_in */
//      uLong    total_in;  /* total nb of input bytes read so far */
//
//      Bytef    *next_out; /* next output byte should be put there */
//      uInt     avail_out; /* remaining free space at next_out */
//      uLong    total_out; /* total nb of bytes output so far */
//
//      char     *msg;      /* last error message, NULL if no error */
//      struct internal_state FAR *state; /* not visible by applications */
//
//      alloc_func zalloc;  /* used to allocate the internal state */
//      free_func  zfree;   /* used to free the internal state */
//      voidpf     opaque;  /* private data object passed to zalloc and zfree */
//
//      int     data_type;  /* best guess about the data type: binary or text */
//      uLong   adler;      /* adler32 value of the uncompressed data */
//      uLong   reserved;   /* reserved for future use */
//  } z_stream;

int GzipDecompressingFileWriter::Write(const void* bytes, size_t count) {
  // Steps:
  // put the data on next_in
  // call inflate until it returns nothing, each time writing what we get
  // check that next_in has no data left.

  // It seems that zlib can keep internal buffers in the stream object,
  // so not all data we get fed to us this time will necessarily
  // be written out this time (in decompressed form).

  if (stream_.avail_in) {
    LOG(ERROR) << "Have data already. Bailing";
    return -1;
  }
  char buf[1024];

  buffer_.reserve(count);
  buffer_.clear();
  CHECK_GE(buffer_.capacity(), count);
  const char* char_bytes = reinterpret_cast<const char*>(bytes);
  buffer_.insert(buffer_.end(), char_bytes, char_bytes + count);

  stream_.next_in = reinterpret_cast<Bytef*>(&buffer_[0]);
  stream_.avail_in = count;
  int retcode = Z_OK;

  while (Z_OK == retcode) {
    stream_.next_out = reinterpret_cast<Bytef*>(buf);
    stream_.avail_out = sizeof(buf);
    int retcode = inflate(&stream_, Z_NO_FLUSH);
    // check for Z_STREAM_END, Z_OK, or Z_BUF_ERROR (which is non-fatal)
    if (Z_STREAM_END != retcode && Z_OK != retcode && Z_BUF_ERROR != retcode) {
      LOG(ERROR) << "zlib inflate() error:" << retcode;
      if (stream_.msg)
        LOG(ERROR) << "message:" << stream_.msg;
      return 0;
    }
    int count_received = sizeof(buf) - stream_.avail_out;
    if (count_received > 0) {
      next_->Write(buf, count_received);
    } else {
      // Inflate returned no data; we're done for now. Make sure no
      // input data remain.
      CHECK_EQ(0, stream_.avail_in);
      break;
    }
  }
  return count;
}

}  // namespace chromeos_update_engine
