// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/bzip_extent_writer.h"

using std::vector;

namespace chromeos_update_engine {

namespace {
const vector<char>::size_type kOutputBufferLength = 1024 * 1024;
}

bool BzipExtentWriter::Init(int fd,
                            const vector<Extent>& extents,
                            uint32 block_size) {
  // Init bzip2 stream
  int rc = BZ2_bzDecompressInit(&stream_,
                                0,  // verbosity. (0 == silent)
                                0  // 0 = faster algo, more memory
                                );
  TEST_AND_RETURN_FALSE(rc == BZ_OK);

  return next_->Init(fd, extents, block_size);
}

bool BzipExtentWriter::Write(const void* bytes, size_t count) {
  vector<char> output_buffer(kOutputBufferLength);

  const char* c_bytes = reinterpret_cast<const char*>(bytes);

  input_buffer_.insert(input_buffer_.end(), c_bytes, c_bytes + count);
  
  stream_.next_in = &input_buffer_[0];
  stream_.avail_in = input_buffer_.size();
  
  for (;;) {
    stream_.next_out = &output_buffer[0];
    stream_.avail_out = output_buffer.size();

    int rc = BZ2_bzDecompress(&stream_);
    TEST_AND_RETURN_FALSE(rc == BZ_OK || rc == BZ_STREAM_END);
    
    if (stream_.avail_out == output_buffer.size())
      break;  // got no new bytes
    
    TEST_AND_RETURN_FALSE(
        next_->Write(&output_buffer[0],
                     output_buffer.size() - stream_.avail_out));
    
    if (rc == BZ_STREAM_END)
      CHECK_EQ(stream_.avail_in, 0);
    if (stream_.avail_in == 0)
      break;  // no more input to process
  }

  // store unconsumed data in input_buffer_.
  
  vector<char> new_input_buffer(input_buffer_.end() - stream_.avail_in,
                                input_buffer_.end());
  new_input_buffer.swap(input_buffer_);
  
  return true;
}

bool BzipExtentWriter::EndImpl() {
  TEST_AND_RETURN_FALSE(input_buffer_.empty());
  TEST_AND_RETURN_FALSE(BZ2_bzDecompressEnd(&stream_) == BZ_OK);
  return next_->End();
}

}  // namespace chromeos_update_engine
