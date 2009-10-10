// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_MOCK_FILE_WRITER_H__
#define UPDATE_ENGINE_MOCK_FILE_WRITER_H__

#include "base/basictypes.h"
#include "update_engine/file_writer.h"

// MockFileWriter is an implementation of FileWriter. It will succeed
// calls to Open(), Close(), but not do any work. All calls to Write()
// will append the passed data to an internal vector.

namespace chromeos_update_engine {

class MockFileWriter : public FileWriter {
 public:
  MockFileWriter() : was_opened_(false), was_closed_(false) {}

  virtual int Open(const char* path, int flags, mode_t mode) {
    CHECK(!was_opened_);
    CHECK(!was_closed_);
    was_opened_ = true;
    return 0;
  }

  virtual int Write(const void* bytes, size_t count) {
    CHECK(was_opened_);
    CHECK(!was_closed_);
    const char* char_bytes = reinterpret_cast<const char*>(bytes);
    bytes_.insert(bytes_.end(), char_bytes, char_bytes + count);
    return count;
  }

  virtual int Close() {
    CHECK(was_opened_);
    CHECK(!was_closed_);
    was_closed_ = true;
    return 0;
  }

  const std::vector<char>& bytes() {
    return bytes_;
  }
 private:
  // The internal store of all bytes that have been written
  std::vector<char> bytes_;

  // These are just to ensure FileWriter methods are called properly.
  bool was_opened_;
  bool was_closed_;

  DISALLOW_COPY_AND_ASSIGN(MockFileWriter);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_MOCK_FILE_WRITER_H__
