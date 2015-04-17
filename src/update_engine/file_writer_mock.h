// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_FILE_WRITER_MOCK_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_FILE_WRITER_MOCK_H__

#include "gmock/gmock.h"
#include "update_engine/file_writer.h"

namespace chromeos_update_engine {

class FileWriterMock : public FileWriter {
 public:
  MOCK_METHOD3(Open, int(const char* path, int flags, mode_t mode));
  MOCK_METHOD2(Write, ssize_t(const void* bytes, size_t count));
  MOCK_METHOD0(Close, int());
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_FILE_WRITER_MOCK_H__
