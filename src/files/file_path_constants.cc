// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "files/file_path.h"

#include "base/basictypes.h" // arraysize

namespace files {

const FilePath::CharType FilePath::kSeparators[] = FILE_PATH_LITERAL("/");

const size_t FilePath::kSeparatorsLength = arraysize(kSeparators);

const FilePath::CharType FilePath::kCurrentDirectory[] = FILE_PATH_LITERAL(".");
const FilePath::CharType FilePath::kParentDirectory[] = FILE_PATH_LITERAL("..");

}  // namespace files
