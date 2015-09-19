// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "files/file_enumerator.h"

namespace files {

FileEnumerator::FileInfo::~FileInfo() {
}

bool FileEnumerator::ShouldSkip(const FilePath& path) {
  FilePath::StringType basename = path.BaseName().value();
  return basename == FILE_PATH_LITERAL(".") ||
         (basename == FILE_PATH_LITERAL("..") &&
          !(INCLUDE_DOT_DOT & file_type_));
}

}  // namespace files
