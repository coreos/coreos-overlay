// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FILES_SCOPED_TEMP_DIR_H_
#define FILES_SCOPED_TEMP_DIR_H_

// An object representing a temporary / scratch directory that should be cleaned
// up (recursively) when this object goes out of scope.  Note that since
// deletion occurs during the destructor, no further error handling is possible
// if the directory fails to be deleted.  As a result, deletion is not
// guaranteed by this class.
//
// Multiple calls to the methods which establish a temporary directory
// (CreateUniqueTempDir, CreateUniqueTempDirUnderPath, and Set) must have
// intervening calls to Delete or Take, or the calls will fail.

#include "files/file_path.h"
#include "macros.h"

namespace files {

class ScopedTempDir {
 public:
  // No directory is owned/created initially.
  ScopedTempDir();

  // Recursively delete path.
  ~ScopedTempDir();

  // Creates a unique directory in TempPath, and takes ownership of it.
  // See file_util::CreateNewTemporaryDirectory.
  [[gnu::warn_unused_result]] bool CreateUniqueTempDir();

  // Creates a unique directory under a given path, and takes ownership of it.
  [[gnu::warn_unused_result]] bool CreateUniqueTempDirUnderPath(const FilePath& path);

  // Takes ownership of directory at |path|, creating it if necessary.
  // Don't call multiple times unless Take() has been called first.
  [[gnu::warn_unused_result]] bool Set(const FilePath& path);

  // Deletes the temporary directory wrapped by this object.
  [[gnu::warn_unused_result]] bool Delete();

  // Caller takes ownership of the temporary directory so it won't be destroyed
  // when this object goes out of scope.
  FilePath Take();

  const FilePath& path() const { return path_; }

  // Returns true if path_ is non-empty and exists.
  bool IsValid() const;

 private:
  FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTempDir);
};

}  // namespace files

#endif  // FILES_SCOPED_TEMP_DIR_H_
