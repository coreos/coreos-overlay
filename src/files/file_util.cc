// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "files/file_util.h"

#include <stdio.h>
#include <unistd.h>

#include <fstream>
#include <limits>
#include <memory>

#include "base/logging.h"

#include "files/file_enumerator.h"
#include "files/file_path.h"

namespace files {

bool Move(const FilePath& from_path, const FilePath& to_path) {
  if (from_path.ReferencesParent() || to_path.ReferencesParent())
    return false;
  return internal::MoveUnsafe(from_path, to_path);
}

bool ReadFileToString(const FilePath& path,
                      std::string* contents,
                      size_t max_size) {
  if (contents)
    contents->clear();
  if (path.ReferencesParent())
    return false;
  FILE* file = OpenFile(path, "rb");
  if (!file) {
    return false;
  }

  const size_t kBufferSize = 1 << 16;
  std::unique_ptr<char[]> buf(new char[kBufferSize]);
  size_t len;
  size_t size = 0;
  bool read_status = true;

  // Many files supplied in |path| have incorrect size (proc files etc).
  // Hence, the file is read sequentially as opposed to a one-shot read.
  while ((len = fread(buf.get(), 1, kBufferSize, file)) > 0) {
    if (contents)
      contents->append(buf.get(), std::min(len, max_size - size));

    if ((max_size - size) < len) {
      read_status = false;
      break;
    }

    size += len;
  }
  read_status = read_status && !ferror(file);
  CloseFile(file);

  return read_status;
}

bool ReadFileToString(const FilePath& path, std::string* contents) {
  return ReadFileToString(path, contents, std::numeric_limits<size_t>::max());
}

bool IsDirectoryEmpty(const FilePath& dir_path) {
  FileEnumerator files(dir_path, false,
      FileEnumerator::FILES | FileEnumerator::DIRECTORIES);
  if (files.Next().empty())
    return true;
  return false;
}

FILE* CreateAndOpenTemporaryFile(FilePath* path) {
  FilePath directory;
  if (!GetTempDir(&directory))
    return NULL;

  return CreateAndOpenTemporaryFileInDir(directory, path);
}

bool CloseFile(FILE* file) {
  if (file == NULL)
    return true;
  return fclose(file) == 0;
}

bool TruncateFile(FILE* file) {
  if (file == NULL)
    return false;
  long current_offset = ftell(file);
  if (current_offset == -1)
    return false;
  int fd = fileno(file);
  if (ftruncate(fd, current_offset) != 0)
    return false;
  return true;
}

}  // namespace files
