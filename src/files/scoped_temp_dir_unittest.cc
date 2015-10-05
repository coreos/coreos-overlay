// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <gtest/gtest.h>

#include "files/file_util.h"
#include "files/scoped_temp_dir.h"

namespace files {

TEST(ScopedTempDir, FullPath) {
  FilePath test_path;
  CreateNewTempDirectory(FILE_PATH_LITERAL("scoped_temp_dir"), &test_path);

  // Against an existing dir, it should get destroyed when leaving scope.
  EXPECT_TRUE(DirectoryExists(test_path));
  {
    ScopedTempDir dir;
    EXPECT_TRUE(dir.Set(test_path));
    EXPECT_TRUE(dir.IsValid());
  }
  EXPECT_FALSE(DirectoryExists(test_path));

  {
    ScopedTempDir dir;
    EXPECT_TRUE(dir.Set(test_path));
    // Now the dir doesn't exist, so ensure that it gets created.
    EXPECT_TRUE(DirectoryExists(test_path));
    // When we call Release(), it shouldn't get destroyed when leaving scope.
    FilePath path = dir.Take();
    EXPECT_EQ(path.value(), test_path.value());
    EXPECT_FALSE(dir.IsValid());
  }
  EXPECT_TRUE(DirectoryExists(test_path));

  // Clean up.
  {
    ScopedTempDir dir;
    EXPECT_TRUE(dir.Set(test_path));
  }
  EXPECT_FALSE(DirectoryExists(test_path));
}

TEST(ScopedTempDir, TempDir) {
  // In this case, just verify that a directory was created and that it's a
  // child of TempDir.
  FilePath test_path;
  {
    ScopedTempDir dir;
    EXPECT_TRUE(dir.CreateUniqueTempDir());
    test_path = dir.path();
    EXPECT_TRUE(DirectoryExists(test_path));
    FilePath tmp_dir;
    EXPECT_TRUE(GetTempDir(&tmp_dir));
    EXPECT_TRUE(test_path.value().find(tmp_dir.value()) != std::string::npos);
  }
  EXPECT_FALSE(DirectoryExists(test_path));
}

TEST(ScopedTempDir, UniqueTempDirUnderPath) {
  // Create a path which will contain a unique temp path.
  FilePath base_path;
  ASSERT_TRUE(CreateNewTempDirectory(FILE_PATH_LITERAL("base_dir"),
                                     &base_path));

  FilePath test_path;
  {
    ScopedTempDir dir;
    EXPECT_TRUE(dir.CreateUniqueTempDirUnderPath(base_path));
    test_path = dir.path();
    EXPECT_TRUE(DirectoryExists(test_path));
    EXPECT_TRUE(base_path.IsParent(test_path));
    EXPECT_TRUE(test_path.value().find(base_path.value()) != std::string::npos);
  }
  EXPECT_FALSE(DirectoryExists(test_path));
  DeleteFile(base_path, true);
}

TEST(ScopedTempDir, MultipleInvocations) {
  ScopedTempDir dir;
  EXPECT_TRUE(dir.CreateUniqueTempDir());
  EXPECT_FALSE(dir.CreateUniqueTempDir());
  EXPECT_TRUE(dir.Delete());
  EXPECT_TRUE(dir.CreateUniqueTempDir());
  EXPECT_FALSE(dir.CreateUniqueTempDir());
  ScopedTempDir other_dir;
  EXPECT_TRUE(other_dir.Set(dir.Take()));
  EXPECT_TRUE(dir.CreateUniqueTempDir());
  EXPECT_FALSE(dir.CreateUniqueTempDir());
  EXPECT_FALSE(other_dir.CreateUniqueTempDir());
}

}  // namespace files
