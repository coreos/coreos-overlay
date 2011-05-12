// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_BUFFERED_FILE_WRITER_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_BUFFERED_FILE_WRITER_H__

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/file_writer.h"

// BufferedFileWriter is an implementation of FileWriter that buffers all data
// before passing it onto another FileWriter.

namespace chromeos_update_engine {

class BufferedFileWriter : public FileWriter {
 public:
  BufferedFileWriter(FileWriter* next, size_t buffer_size);
  virtual ~BufferedFileWriter();

  virtual int Open(const char* path, int flags, mode_t mode);
  virtual ssize_t Write(const void* bytes, size_t count);
  virtual int Close();

 private:
  FRIEND_TEST(BufferedFileWriterTest, CloseErrorTest);
  FRIEND_TEST(BufferedFileWriterTest, CloseNoDataTest);
  FRIEND_TEST(BufferedFileWriterTest, CloseWriteErrorTest);
  FRIEND_TEST(BufferedFileWriterTest, ConstructorTest);
  FRIEND_TEST(BufferedFileWriterTest, WriteBufferNoDataTest);
  FRIEND_TEST(BufferedFileWriterTest, WriteBufferTest);
  FRIEND_TEST(BufferedFileWriterTest, WriteErrorTest);
  FRIEND_TEST(BufferedFileWriterTest, WriteTest);
  FRIEND_TEST(BufferedFileWriterTest, WriteWrapBufferTest);

  // If non-empty, sends the buffer to the next FileWriter.
  ssize_t WriteBuffer();

  // The FileWriter that we write all buffered data to.
  FileWriter* const next_;

  scoped_array<char> buffer_;
  size_t buffer_size_;
  size_t buffered_bytes_;

  DISALLOW_COPY_AND_ASSIGN(BufferedFileWriter);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_BUFFERED_FILE_WRITER_H__
