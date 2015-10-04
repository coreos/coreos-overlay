// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <fstream>
#include <set>
#include <vector>

#include <gtest/gtest.h>

#include "files/file_enumerator.h"
#include "files/file_path.h"
#include "files/file_util.h"
#include "files/scoped_temp_dir.h"

// This macro helps avoid wrapped lines in the test structs.
#define FPL(x) FILE_PATH_LITERAL(x)

namespace files {

namespace {

const wchar_t bogus_content[] = L"I'm cannon fodder.";

const int FILES_AND_DIRECTORIES =
    FileEnumerator::FILES | FileEnumerator::DIRECTORIES;

class FileUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  ScopedTempDir temp_dir_;
};

// Collects all the results from the given file enumerator, and provides an
// interface to query whether a given file is present.
class FindResultCollector {
 public:
  explicit FindResultCollector(FileEnumerator* enumerator) {
    FilePath cur_file;
    while (!(cur_file = enumerator->Next()).value().empty()) {
      FilePath::StringType path = cur_file.value();
      // The file should not be returned twice.
      EXPECT_TRUE(files_.end() == files_.find(path))
          << "Same file returned twice";

      // Save for later.
      files_.insert(path);
    }
  }

  // Returns true if the enumerator found the file.
  bool HasFile(const FilePath& file) const {
    return files_.find(file.value()) != files_.end();
  }

  int size() {
    return static_cast<int>(files_.size());
  }

 private:
  std::set<FilePath::StringType> files_;
};

// Simple function to dump some text into a new file.
void CreateTextFile(const FilePath& filename,
                    const std::wstring& contents) {
  std::wofstream file;
  file.open(filename.value().c_str());
  ASSERT_TRUE(file.is_open());
  file << contents;
  file.close();
}

// Simple function to take out some text from a file.
std::wstring ReadTextFile(const FilePath& filename) {
  wchar_t contents[64];
  std::wifstream file;
  file.open(filename.value().c_str());
  EXPECT_TRUE(file.is_open());
  file.getline(contents, arraysize(contents));
  file.close();
  return std::wstring(contents);
}

TEST_F(FileUtilTest, CreateAndReadSymlinks) {
  FilePath link_from = temp_dir_.path().Append(FPL("from_file"));
  FilePath link_to = temp_dir_.path().Append(FPL("to_file"));
  CreateTextFile(link_to, bogus_content);

  ASSERT_TRUE(CreateSymbolicLink(link_to, link_from))
    << "Failed to create file symlink.";

  // If we created the link properly, we should be able to read the contents
  // through it.
  std::wstring contents = ReadTextFile(link_from);
  EXPECT_EQ(bogus_content, contents);

  FilePath result;
  ASSERT_TRUE(ReadSymbolicLink(link_from, &result));
  EXPECT_EQ(link_to.value(), result.value());

  // Link to a directory.
  link_from = temp_dir_.path().Append(FPL("from_dir"));
  link_to = temp_dir_.path().Append(FPL("to_dir"));
  ASSERT_TRUE(CreateDirectory(link_to));
  ASSERT_TRUE(CreateSymbolicLink(link_to, link_from))
    << "Failed to create directory symlink.";

  // Test failures.
  EXPECT_FALSE(CreateSymbolicLink(link_to, link_to));
  EXPECT_FALSE(ReadSymbolicLink(link_to, &result));
  FilePath missing = temp_dir_.path().Append(FPL("missing"));
  EXPECT_FALSE(ReadSymbolicLink(missing, &result));
}

TEST_F(FileUtilTest, DeleteNonExistent) {
  FilePath non_existent = temp_dir_.path().Append("bogus_file_dne.foobar");
  ASSERT_FALSE(PathExists(non_existent));

  EXPECT_TRUE(DeleteFile(non_existent, false));
  ASSERT_FALSE(PathExists(non_existent));
  EXPECT_TRUE(DeleteFile(non_existent, true));
  ASSERT_FALSE(PathExists(non_existent));
}

TEST_F(FileUtilTest, DeleteNonExistentWithNonExistentParent) {
  FilePath non_existent = temp_dir_.path().Append("bogus_topdir");
  non_existent = non_existent.Append("bogus_subdir");
  ASSERT_FALSE(PathExists(non_existent));

  EXPECT_TRUE(DeleteFile(non_existent, false));
  ASSERT_FALSE(PathExists(non_existent));
  EXPECT_TRUE(DeleteFile(non_existent, true));
  ASSERT_FALSE(PathExists(non_existent));
}

TEST_F(FileUtilTest, DeleteFile) {
  // Create a file
  FilePath file_name = temp_dir_.path().Append(FPL("Test DeleteFile 1.txt"));
  CreateTextFile(file_name, bogus_content);
  ASSERT_TRUE(PathExists(file_name));

  // Make sure it's deleted
  EXPECT_TRUE(DeleteFile(file_name, false));
  EXPECT_FALSE(PathExists(file_name));

  // Test recursive case, create a new file
  file_name = temp_dir_.path().Append(FPL("Test DeleteFile 2.txt"));
  CreateTextFile(file_name, bogus_content);
  ASSERT_TRUE(PathExists(file_name));

  // Make sure it's deleted
  EXPECT_TRUE(DeleteFile(file_name, true));
  EXPECT_FALSE(PathExists(file_name));
}

TEST_F(FileUtilTest, DeleteSymlinkToExistentFile) {
  // Create a file.
  FilePath file_name = temp_dir_.path().Append(FPL("Test DeleteFile 2.txt"));
  CreateTextFile(file_name, bogus_content);
  ASSERT_TRUE(PathExists(file_name));

  // Create a symlink to the file.
  FilePath file_link = temp_dir_.path().Append("file_link_2");
  ASSERT_TRUE(CreateSymbolicLink(file_name, file_link))
      << "Failed to create symlink.";

  // Delete the symbolic link.
  EXPECT_TRUE(DeleteFile(file_link, false));

  // Make sure original file is not deleted.
  EXPECT_FALSE(PathExists(file_link));
  EXPECT_TRUE(PathExists(file_name));
}

TEST_F(FileUtilTest, DeleteSymlinkToNonExistentFile) {
  // Create a non-existent file path.
  FilePath non_existent = temp_dir_.path().Append(FPL("Test DeleteFile 3.txt"));
  EXPECT_FALSE(PathExists(non_existent));

  // Create a symlink to the non-existent file.
  FilePath file_link = temp_dir_.path().Append("file_link_3");
  ASSERT_TRUE(CreateSymbolicLink(non_existent, file_link))
      << "Failed to create symlink.";

  // Make sure the symbolic link is exist.
  EXPECT_TRUE(IsLink(file_link));
  EXPECT_FALSE(PathExists(file_link));

  // Delete the symbolic link.
  EXPECT_TRUE(DeleteFile(file_link, false));

  // Make sure the symbolic link is deleted.
  EXPECT_FALSE(IsLink(file_link));
}

TEST_F(FileUtilTest, ChangeFilePermissionsAndRead) {
  // Create a file path.
  FilePath file_name = temp_dir_.path().Append(FPL("Test Readable File.txt"));
  EXPECT_FALSE(PathExists(file_name));

  const std::string kData("hello");

  int buffer_size = kData.length();
  char* buffer = new char[buffer_size];

  // Write file.
  EXPECT_EQ(static_cast<int>(kData.length()),
            WriteFile(file_name, kData.data(), kData.length()));
  EXPECT_TRUE(PathExists(file_name));

  // Make sure the file is readable.
  int mode = 0;
  EXPECT_TRUE(GetPosixFilePermissions(file_name, &mode));
  EXPECT_TRUE(mode & FILE_PERMISSION_READ_BY_USER);

  // Get rid of the read permission.
  EXPECT_TRUE(SetPosixFilePermissions(file_name, 0u));
  EXPECT_TRUE(GetPosixFilePermissions(file_name, &mode));
  EXPECT_FALSE(mode & FILE_PERMISSION_READ_BY_USER);
  // Make sure the file can't be read.
  EXPECT_EQ(-1, ReadFile(file_name, buffer, buffer_size));

  // Give the read permission.
  EXPECT_TRUE(SetPosixFilePermissions(file_name, FILE_PERMISSION_READ_BY_USER));
  EXPECT_TRUE(GetPosixFilePermissions(file_name, &mode));
  EXPECT_TRUE(mode & FILE_PERMISSION_READ_BY_USER);
  // Make sure the file can be read.
  EXPECT_EQ(static_cast<int>(kData.length()),
            ReadFile(file_name, buffer, buffer_size));

  // Delete the file.
  EXPECT_TRUE(DeleteFile(file_name, false));
  EXPECT_FALSE(PathExists(file_name));

  delete[] buffer;
}

TEST_F(FileUtilTest, ChangeFilePermissionsAndWrite) {
  // Create a file path.
  FilePath file_name = temp_dir_.path().Append(FPL("Test Readable File.txt"));
  EXPECT_FALSE(PathExists(file_name));

  const std::string kData("hello");

  // Write file.
  EXPECT_EQ(static_cast<int>(kData.length()),
            WriteFile(file_name, kData.data(), kData.length()));
  EXPECT_TRUE(PathExists(file_name));

  // Make sure the file is writable.
  int mode = 0;
  EXPECT_TRUE(GetPosixFilePermissions(file_name, &mode));
  EXPECT_TRUE(mode & FILE_PERMISSION_WRITE_BY_USER);
  EXPECT_TRUE(PathIsWritable(file_name));

  // Get rid of the write permission.
  EXPECT_TRUE(SetPosixFilePermissions(file_name, 0u));
  EXPECT_TRUE(GetPosixFilePermissions(file_name, &mode));
  EXPECT_FALSE(mode & FILE_PERMISSION_WRITE_BY_USER);
  // Make sure the file can't be write.
  EXPECT_EQ(-1, WriteFile(file_name, kData.data(), kData.length()));
  EXPECT_FALSE(PathIsWritable(file_name));

  // Give read permission.
  EXPECT_TRUE(SetPosixFilePermissions(file_name,
                                      FILE_PERMISSION_WRITE_BY_USER));
  EXPECT_TRUE(GetPosixFilePermissions(file_name, &mode));
  EXPECT_TRUE(mode & FILE_PERMISSION_WRITE_BY_USER);
  // Make sure the file can be write.
  EXPECT_EQ(static_cast<int>(kData.length()),
            WriteFile(file_name, kData.data(), kData.length()));
  EXPECT_TRUE(PathIsWritable(file_name));

  // Delete the file.
  EXPECT_TRUE(DeleteFile(file_name, false));
  EXPECT_FALSE(PathExists(file_name));
}

TEST_F(FileUtilTest, ChangeDirectoryPermissionsAndEnumerate) {
  // Create a directory path.
  FilePath subdir_path =
      temp_dir_.path().Append(FPL("PermissionTest1"));
  CreateDirectory(subdir_path);
  ASSERT_TRUE(PathExists(subdir_path));

  // Create a dummy file to enumerate.
  FilePath file_name = subdir_path.Append(FPL("Test Readable File.txt"));
  EXPECT_FALSE(PathExists(file_name));
  const std::string kData("hello");
  EXPECT_EQ(static_cast<int>(kData.length()),
            WriteFile(file_name, kData.data(), kData.length()));
  EXPECT_TRUE(PathExists(file_name));

  // Make sure the directory has the all permissions.
  int mode = 0;
  EXPECT_TRUE(GetPosixFilePermissions(subdir_path, &mode));
  EXPECT_EQ(FILE_PERMISSION_USER_MASK, mode & FILE_PERMISSION_USER_MASK);

  // Get rid of the permissions from the directory.
  EXPECT_TRUE(SetPosixFilePermissions(subdir_path, 0u));
  EXPECT_TRUE(GetPosixFilePermissions(subdir_path, &mode));
  EXPECT_FALSE(mode & FILE_PERMISSION_USER_MASK);

  // Make sure the file in the directory can't be enumerated.
  FileEnumerator f1(subdir_path, true, FileEnumerator::FILES);
  EXPECT_TRUE(PathExists(subdir_path));
  FindResultCollector c1(&f1);
  EXPECT_EQ(0, c1.size());
  EXPECT_FALSE(GetPosixFilePermissions(file_name, &mode));

  // Give the permissions to the directory.
  EXPECT_TRUE(SetPosixFilePermissions(subdir_path, FILE_PERMISSION_USER_MASK));
  EXPECT_TRUE(GetPosixFilePermissions(subdir_path, &mode));
  EXPECT_EQ(FILE_PERMISSION_USER_MASK, mode & FILE_PERMISSION_USER_MASK);

  // Make sure the file in the directory can be enumerated.
  FileEnumerator f2(subdir_path, true, FileEnumerator::FILES);
  FindResultCollector c2(&f2);
  EXPECT_TRUE(c2.HasFile(file_name));
  EXPECT_EQ(1, c2.size());

  // Delete the file.
  EXPECT_TRUE(DeleteFile(subdir_path, true));
  EXPECT_FALSE(PathExists(subdir_path));
}

// Tests non-recursive Delete() for a directory.
TEST_F(FileUtilTest, DeleteDirNonRecursive) {
  // Create a subdirectory and put a file and two directories inside.
  FilePath test_subdir = temp_dir_.path().Append(FPL("DeleteDirNonRecursive"));
  CreateDirectory(test_subdir);
  ASSERT_TRUE(PathExists(test_subdir));

  FilePath file_name = test_subdir.Append(FPL("Test DeleteDir.txt"));
  CreateTextFile(file_name, bogus_content);
  ASSERT_TRUE(PathExists(file_name));

  FilePath subdir_path1 = test_subdir.Append(FPL("TestSubDir1"));
  CreateDirectory(subdir_path1);
  ASSERT_TRUE(PathExists(subdir_path1));

  FilePath subdir_path2 = test_subdir.Append(FPL("TestSubDir2"));
  CreateDirectory(subdir_path2);
  ASSERT_TRUE(PathExists(subdir_path2));

  // Delete non-recursively and check that the empty dir got deleted
  EXPECT_TRUE(DeleteFile(subdir_path2, false));
  EXPECT_FALSE(PathExists(subdir_path2));

  // Delete non-recursively and check that nothing got deleted
  EXPECT_FALSE(DeleteFile(test_subdir, false));
  EXPECT_TRUE(PathExists(test_subdir));
  EXPECT_TRUE(PathExists(file_name));
  EXPECT_TRUE(PathExists(subdir_path1));
}

// Tests recursive Delete() for a directory.
TEST_F(FileUtilTest, DeleteDirRecursive) {
  // Create a subdirectory and put a file and two directories inside.
  FilePath test_subdir = temp_dir_.path().Append(FPL("DeleteDirRecursive"));
  CreateDirectory(test_subdir);
  ASSERT_TRUE(PathExists(test_subdir));

  FilePath file_name = test_subdir.Append(FPL("Test DeleteDirRecursive.txt"));
  CreateTextFile(file_name, bogus_content);
  ASSERT_TRUE(PathExists(file_name));

  FilePath subdir_path1 = test_subdir.Append(FPL("TestSubDir1"));
  CreateDirectory(subdir_path1);
  ASSERT_TRUE(PathExists(subdir_path1));

  FilePath subdir_path2 = test_subdir.Append(FPL("TestSubDir2"));
  CreateDirectory(subdir_path2);
  ASSERT_TRUE(PathExists(subdir_path2));

  // Delete recursively and check that the empty dir got deleted
  EXPECT_TRUE(DeleteFile(subdir_path2, true));
  EXPECT_FALSE(PathExists(subdir_path2));

  // Delete recursively and check that everything got deleted
  EXPECT_TRUE(DeleteFile(test_subdir, true));
  EXPECT_FALSE(PathExists(file_name));
  EXPECT_FALSE(PathExists(subdir_path1));
  EXPECT_FALSE(PathExists(test_subdir));
}

TEST_F(FileUtilTest, MoveFileNew) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Move_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // The destination.
  FilePath file_name_to = temp_dir_.path().Append(
      FILE_PATH_LITERAL("Move_Test_File_Destination.txt"));
  ASSERT_FALSE(PathExists(file_name_to));

  EXPECT_TRUE(Move(file_name_from, file_name_to));

  // Check everything has been moved.
  EXPECT_FALSE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(file_name_to));
}

TEST_F(FileUtilTest, MoveFileExists) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Move_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // The destination name.
  FilePath file_name_to = temp_dir_.path().Append(
      FILE_PATH_LITERAL("Move_Test_File_Destination.txt"));
  CreateTextFile(file_name_to, L"Old file content");
  ASSERT_TRUE(PathExists(file_name_to));

  EXPECT_TRUE(Move(file_name_from, file_name_to));

  // Check everything has been moved.
  EXPECT_FALSE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(file_name_to));
  EXPECT_TRUE(L"Gooooooooooooooooooooogle" == ReadTextFile(file_name_to));
}

TEST_F(FileUtilTest, MoveFileDirExists) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Move_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // The destination directory
  FilePath dir_name_to =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Destination"));
  CreateDirectory(dir_name_to);
  ASSERT_TRUE(PathExists(dir_name_to));

  EXPECT_FALSE(Move(file_name_from, dir_name_to));
}


TEST_F(FileUtilTest, MoveNew) {
  // Create a directory
  FilePath dir_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Move_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory
  FilePath txt_file_name(FILE_PATH_LITERAL("Move_Test_File.txt"));
  FilePath file_name_from = dir_name_from.Append(txt_file_name);
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Move the directory.
  FilePath dir_name_to =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Move_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Move_Test_File.txt"));

  ASSERT_FALSE(PathExists(dir_name_to));

  EXPECT_TRUE(Move(dir_name_from, dir_name_to));

  // Check everything has been moved.
  EXPECT_FALSE(PathExists(dir_name_from));
  EXPECT_FALSE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(dir_name_to));
  EXPECT_TRUE(PathExists(file_name_to));

  // Test path traversal.
  file_name_from = dir_name_to.Append(txt_file_name);
  file_name_to = dir_name_to.Append(FILE_PATH_LITERAL(".."));
  file_name_to = file_name_to.Append(txt_file_name);
  EXPECT_FALSE(Move(file_name_from, file_name_to));
  EXPECT_TRUE(PathExists(file_name_from));
  EXPECT_FALSE(PathExists(file_name_to));
  EXPECT_TRUE(internal::MoveUnsafe(file_name_from, file_name_to));
  EXPECT_FALSE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(file_name_to));
}

TEST_F(FileUtilTest, MoveExist) {
  // Create a directory
  FilePath dir_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Move_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Move_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Move the directory
  FilePath dir_name_exists =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Destination"));

  FilePath dir_name_to =
      dir_name_exists.Append(FILE_PATH_LITERAL("Move_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Move_Test_File.txt"));

  // Create the destination directory.
  CreateDirectory(dir_name_exists);
  ASSERT_TRUE(PathExists(dir_name_exists));

  EXPECT_TRUE(Move(dir_name_from, dir_name_to));

  // Check everything has been moved.
  EXPECT_FALSE(PathExists(dir_name_from));
  EXPECT_FALSE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(dir_name_to));
  EXPECT_TRUE(PathExists(file_name_to));
}

TEST_F(FileUtilTest, CopyDirectoryRecursivelyNew) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Subdir"));
  CreateDirectory(subdir_name_from);
  ASSERT_TRUE(PathExists(subdir_name_from));

  // Create a file under the subdirectory.
  FilePath file_name2_from =
      subdir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name2_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name2_from));

  // Copy the directory recursively.
  FilePath dir_name_to =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  FilePath subdir_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Subdir"));
  FilePath file_name2_to =
      subdir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));

  ASSERT_FALSE(PathExists(dir_name_to));

  EXPECT_TRUE(CopyDirectory(dir_name_from, dir_name_to, true));

  // Check everything has been copied.
  EXPECT_TRUE(PathExists(dir_name_from));
  EXPECT_TRUE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(subdir_name_from));
  EXPECT_TRUE(PathExists(file_name2_from));
  EXPECT_TRUE(PathExists(dir_name_to));
  EXPECT_TRUE(PathExists(file_name_to));
  EXPECT_TRUE(PathExists(subdir_name_to));
  EXPECT_TRUE(PathExists(file_name2_to));
}

TEST_F(FileUtilTest, CopyDirectoryRecursivelyExists) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Subdir"));
  CreateDirectory(subdir_name_from);
  ASSERT_TRUE(PathExists(subdir_name_from));

  // Create a file under the subdirectory.
  FilePath file_name2_from =
      subdir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name2_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name2_from));

  // Copy the directory recursively.
  FilePath dir_name_exists =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Destination"));

  FilePath dir_name_to =
      dir_name_exists.Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  FilePath subdir_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Subdir"));
  FilePath file_name2_to =
      subdir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));

  // Create the destination directory.
  CreateDirectory(dir_name_exists);
  ASSERT_TRUE(PathExists(dir_name_exists));

  EXPECT_TRUE(CopyDirectory(dir_name_from, dir_name_exists, true));

  // Check everything has been copied.
  EXPECT_TRUE(PathExists(dir_name_from));
  EXPECT_TRUE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(subdir_name_from));
  EXPECT_TRUE(PathExists(file_name2_from));
  EXPECT_TRUE(PathExists(dir_name_to));
  EXPECT_TRUE(PathExists(file_name_to));
  EXPECT_TRUE(PathExists(subdir_name_to));
  EXPECT_TRUE(PathExists(file_name2_to));
}

TEST_F(FileUtilTest, CopyDirectoryNew) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Subdir"));
  CreateDirectory(subdir_name_from);
  ASSERT_TRUE(PathExists(subdir_name_from));

  // Create a file under the subdirectory.
  FilePath file_name2_from =
      subdir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name2_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name2_from));

  // Copy the directory not recursively.
  FilePath dir_name_to =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  FilePath subdir_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Subdir"));

  ASSERT_FALSE(PathExists(dir_name_to));

  EXPECT_TRUE(CopyDirectory(dir_name_from, dir_name_to, false));

  // Check everything has been copied.
  EXPECT_TRUE(PathExists(dir_name_from));
  EXPECT_TRUE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(subdir_name_from));
  EXPECT_TRUE(PathExists(file_name2_from));
  EXPECT_TRUE(PathExists(dir_name_to));
  EXPECT_TRUE(PathExists(file_name_to));
  EXPECT_FALSE(PathExists(subdir_name_to));
}

TEST_F(FileUtilTest, CopyDirectoryExists) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Subdir"));
  CreateDirectory(subdir_name_from);
  ASSERT_TRUE(PathExists(subdir_name_from));

  // Create a file under the subdirectory.
  FilePath file_name2_from =
      subdir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name2_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name2_from));

  // Copy the directory not recursively.
  FilePath dir_name_to =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  FilePath subdir_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Subdir"));

  // Create the destination directory.
  CreateDirectory(dir_name_to);
  ASSERT_TRUE(PathExists(dir_name_to));

  EXPECT_TRUE(CopyDirectory(dir_name_from, dir_name_to, false));

  // Check everything has been copied.
  EXPECT_TRUE(PathExists(dir_name_from));
  EXPECT_TRUE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(subdir_name_from));
  EXPECT_TRUE(PathExists(file_name2_from));
  EXPECT_TRUE(PathExists(dir_name_to));
  EXPECT_TRUE(PathExists(file_name_to));
  EXPECT_FALSE(PathExists(subdir_name_to));
}

TEST_F(FileUtilTest, CopyFileWithCopyDirectoryRecursiveToNew) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // The destination name
  FilePath file_name_to = temp_dir_.path().Append(
      FILE_PATH_LITERAL("Copy_Test_File_Destination.txt"));
  ASSERT_FALSE(PathExists(file_name_to));

  EXPECT_TRUE(CopyDirectory(file_name_from, file_name_to, true));

  // Check the has been copied
  EXPECT_TRUE(PathExists(file_name_to));
}

TEST_F(FileUtilTest, CopyFileWithCopyDirectoryRecursiveToExisting) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // The destination name
  FilePath file_name_to = temp_dir_.path().Append(
      FILE_PATH_LITERAL("Copy_Test_File_Destination.txt"));
  CreateTextFile(file_name_to, L"Old file content");
  ASSERT_TRUE(PathExists(file_name_to));

  EXPECT_TRUE(CopyDirectory(file_name_from, file_name_to, true));

  // Check the has been copied
  EXPECT_TRUE(PathExists(file_name_to));
  EXPECT_TRUE(L"Gooooooooooooooooooooogle" == ReadTextFile(file_name_to));
}

TEST_F(FileUtilTest, CopyFileWithCopyDirectoryRecursiveToExistingDirectory) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // The destination
  FilePath dir_name_to =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Destination"));
  CreateDirectory(dir_name_to);
  ASSERT_TRUE(PathExists(dir_name_to));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));

  EXPECT_TRUE(CopyDirectory(file_name_from, dir_name_to, true));

  // Check the has been copied
  EXPECT_TRUE(PathExists(file_name_to));
}

TEST_F(FileUtilTest, CopyDirectoryWithTrailingSeparators) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Copy the directory recursively.
  FilePath dir_name_to =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));

  // Create from path with trailing separators.
  FilePath from_path =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_From_Subdir///"));

  EXPECT_TRUE(CopyDirectory(from_path, dir_name_to, true));

  // Check everything has been copied.
  EXPECT_TRUE(PathExists(dir_name_from));
  EXPECT_TRUE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(dir_name_to));
  EXPECT_TRUE(PathExists(file_name_to));
}

// Sets the source file to read-only.
void SetReadOnly(const FilePath& path, bool read_only) {
  // On all other platforms, it involves removing/setting the write bit.
  mode_t mode = read_only ? S_IRUSR : (S_IRUSR | S_IWUSR);
  EXPECT_TRUE(SetPosixFilePermissions(
      path, DirectoryExists(path) ? (mode | S_IXUSR) : mode));
}

bool IsReadOnly(const FilePath& path) {
  int mode = 0;
  EXPECT_TRUE(GetPosixFilePermissions(path, &mode));
  return !(mode & S_IWUSR);
}

TEST_F(FileUtilTest, CopyDirectoryACL) {
  // Create source directories.
  FilePath src = temp_dir_.path().Append(FILE_PATH_LITERAL("src"));
  FilePath src_subdir = src.Append(FILE_PATH_LITERAL("subdir"));
  CreateDirectory(src_subdir);
  ASSERT_TRUE(PathExists(src_subdir));

  // Create a file under the directory.
  FilePath src_file = src.Append(FILE_PATH_LITERAL("src.txt"));
  CreateTextFile(src_file, L"Gooooooooooooooooooooogle");
  SetReadOnly(src_file, true);
  ASSERT_TRUE(IsReadOnly(src_file));

  // Make directory read-only.
  SetReadOnly(src_subdir, true);
  ASSERT_TRUE(IsReadOnly(src_subdir));

  // Copy the directory recursively.
  FilePath dst = temp_dir_.path().Append(FILE_PATH_LITERAL("dst"));
  FilePath dst_file = dst.Append(FILE_PATH_LITERAL("src.txt"));
  EXPECT_TRUE(CopyDirectory(src, dst, true));

  FilePath dst_subdir = dst.Append(FILE_PATH_LITERAL("subdir"));
  ASSERT_FALSE(IsReadOnly(dst_subdir));
  ASSERT_FALSE(IsReadOnly(dst_file));

  // Give write permissions to allow deletion.
  SetReadOnly(src_subdir, false);
  ASSERT_FALSE(IsReadOnly(src_subdir));
}

TEST_F(FileUtilTest, CopyFile) {
  // Create a directory
  FilePath dir_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  const std::wstring file_contents(L"Gooooooooooooooooooooogle");
  CreateTextFile(file_name_from, file_contents);
  ASSERT_TRUE(PathExists(file_name_from));

  // Copy the file.
  FilePath dest_file = dir_name_from.Append(FILE_PATH_LITERAL("DestFile.txt"));
  ASSERT_TRUE(CopyFile(file_name_from, dest_file));

  // Try to copy the file to another location using '..' in the path.
  FilePath dest_file2(dir_name_from);
  dest_file2 = dest_file2.Append("..");
  dest_file2 = dest_file2.Append("DestFile.txt");
  ASSERT_FALSE(CopyFile(file_name_from, dest_file2));

  FilePath dest_file2_test(dir_name_from);
  dest_file2_test = dest_file2_test.DirName();
  dest_file2_test = dest_file2_test.Append("DestFile.txt");

  // Check expected copy results.
  EXPECT_TRUE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(dest_file));
  const std::wstring read_contents = ReadTextFile(dest_file);
  EXPECT_EQ(file_contents, read_contents);
  EXPECT_FALSE(PathExists(dest_file2_test));
  EXPECT_FALSE(PathExists(dest_file2));
}

TEST_F(FileUtilTest, CopyFileACL) {
  // While FileUtilTest.CopyFile asserts the content is correctly copied over,
  // this test case asserts the access control bits are meeting expectations in
  // CopyFile().
  FilePath src = temp_dir_.path().Append(FILE_PATH_LITERAL("src.txt"));
  const std::wstring file_contents(L"Gooooooooooooooooooooogle");
  CreateTextFile(src, file_contents);

  // Set the source file to read-only.
  ASSERT_FALSE(IsReadOnly(src));
  SetReadOnly(src, true);
  ASSERT_TRUE(IsReadOnly(src));

  // Copy the file.
  FilePath dst = temp_dir_.path().Append(FILE_PATH_LITERAL("dst.txt"));
  ASSERT_TRUE(CopyFile(src, dst));
  EXPECT_EQ(file_contents, ReadTextFile(dst));

  ASSERT_FALSE(IsReadOnly(dst));
}

TEST_F(FileUtilTest, CreateTemporaryFileTest) {
  FilePath temp_files[3];
  for (int i = 0; i < 3; i++) {
    ASSERT_TRUE(CreateTemporaryFile(&(temp_files[i])));
    EXPECT_TRUE(PathExists(temp_files[i]));
    EXPECT_FALSE(DirectoryExists(temp_files[i]));
  }
  for (int i = 0; i < 3; i++)
    EXPECT_FALSE(temp_files[i] == temp_files[(i+1)%3]);
  for (int i = 0; i < 3; i++)
    EXPECT_TRUE(DeleteFile(temp_files[i], false));
}

TEST_F(FileUtilTest, CreateAndOpenTemporaryFileTest) {
  FilePath names[3];
  FILE* fps[3];
  int i;

  // Create; make sure they are open and exist.
  for (i = 0; i < 3; ++i) {
    fps[i] = CreateAndOpenTemporaryFile(&(names[i]));
    ASSERT_TRUE(fps[i]);
    EXPECT_TRUE(PathExists(names[i]));
  }

  // Make sure all names are unique.
  for (i = 0; i < 3; ++i) {
    EXPECT_FALSE(names[i] == names[(i+1)%3]);
  }

  // Close and delete.
  for (i = 0; i < 3; ++i) {
    EXPECT_TRUE(CloseFile(fps[i]));
    EXPECT_TRUE(DeleteFile(names[i], false));
  }
}

TEST_F(FileUtilTest, CreateNewTempDirectoryTest) {
  FilePath temp_dir;
  ASSERT_TRUE(CreateNewTempDirectory(FilePath::StringType(), &temp_dir));
  EXPECT_TRUE(PathExists(temp_dir));
  EXPECT_TRUE(DeleteFile(temp_dir, false));
}

TEST_F(FileUtilTest, CreateNewTemporaryDirInDirTest) {
  FilePath new_dir;
  ASSERT_TRUE(CreateTemporaryDirInDir(
                  temp_dir_.path(),
                  FILE_PATH_LITERAL("CreateNewTemporaryDirInDirTest"),
                  &new_dir));
  EXPECT_TRUE(PathExists(new_dir));
  EXPECT_TRUE(temp_dir_.path().IsParent(new_dir));
  EXPECT_TRUE(DeleteFile(new_dir, false));
}

TEST_F(FileUtilTest, CreateDirectoryTest) {
  FilePath test_root =
      temp_dir_.path().Append(FILE_PATH_LITERAL("create_directory_test"));
  FilePath test_path =
      test_root.Append(FILE_PATH_LITERAL("dir/tree/likely/doesnt/exist/"));

  EXPECT_FALSE(PathExists(test_path));
  EXPECT_TRUE(CreateDirectory(test_path));
  EXPECT_TRUE(PathExists(test_path));
  // CreateDirectory returns true if the DirectoryExists returns true.
  EXPECT_TRUE(CreateDirectory(test_path));

  // Doesn't work to create it on top of a non-dir
  test_path = test_path.Append(FILE_PATH_LITERAL("foobar.txt"));
  EXPECT_FALSE(PathExists(test_path));
  CreateTextFile(test_path, L"test file");
  EXPECT_TRUE(PathExists(test_path));
  EXPECT_FALSE(CreateDirectory(test_path));

  EXPECT_TRUE(DeleteFile(test_root, true));
  EXPECT_FALSE(PathExists(test_root));
  EXPECT_FALSE(PathExists(test_path));

  // Verify assumptions made by the Windows implementation:
  // 1. The current directory always exists.
  // 2. The root directory always exists.
  ASSERT_TRUE(DirectoryExists(FilePath(FilePath::kCurrentDirectory)));
  FilePath top_level = test_root;
  while (top_level != top_level.DirName()) {
    top_level = top_level.DirName();
  }
  ASSERT_TRUE(DirectoryExists(top_level));

  // Given these assumptions hold, it should be safe to
  // test that "creating" these directories succeeds.
  EXPECT_TRUE(CreateDirectory(
      FilePath(FilePath::kCurrentDirectory)));
  EXPECT_TRUE(CreateDirectory(top_level));
}

TEST_F(FileUtilTest, DetectDirectoryTest) {
  // Check a directory
  FilePath test_root =
      temp_dir_.path().Append(FILE_PATH_LITERAL("detect_directory_test"));
  EXPECT_FALSE(PathExists(test_root));
  EXPECT_TRUE(CreateDirectory(test_root));
  EXPECT_TRUE(PathExists(test_root));
  EXPECT_TRUE(DirectoryExists(test_root));
  // Check a file
  FilePath test_path =
      test_root.Append(FILE_PATH_LITERAL("foobar.txt"));
  EXPECT_FALSE(PathExists(test_path));
  CreateTextFile(test_path, L"test file");
  EXPECT_TRUE(PathExists(test_path));
  EXPECT_FALSE(DirectoryExists(test_path));
  EXPECT_TRUE(DeleteFile(test_path, false));

  EXPECT_TRUE(DeleteFile(test_root, true));
}

TEST_F(FileUtilTest, FileEnumeratorTest) {
  // Test an empty directory.
  FileEnumerator f0(temp_dir_.path(), true, FILES_AND_DIRECTORIES);
  EXPECT_EQ(FPL(""), f0.Next().value());
  EXPECT_EQ(FPL(""), f0.Next().value());

  // Test an empty directory, non-recursively, including "..".
  FileEnumerator f0_dotdot(temp_dir_.path(), false,
      FILES_AND_DIRECTORIES | FileEnumerator::INCLUDE_DOT_DOT);
  EXPECT_EQ(temp_dir_.path().Append(FPL("..")).value(),
            f0_dotdot.Next().value());
  EXPECT_EQ(FPL(""), f0_dotdot.Next().value());

  // create the directories
  FilePath dir1 = temp_dir_.path().Append(FPL("dir1"));
  EXPECT_TRUE(CreateDirectory(dir1));
  FilePath dir2 = temp_dir_.path().Append(FPL("dir2"));
  EXPECT_TRUE(CreateDirectory(dir2));
  FilePath dir2inner = dir2.Append(FPL("inner"));
  EXPECT_TRUE(CreateDirectory(dir2inner));

  // create the files
  FilePath dir2file = dir2.Append(FPL("dir2file.txt"));
  CreateTextFile(dir2file, std::wstring());
  FilePath dir2innerfile = dir2inner.Append(FPL("innerfile.txt"));
  CreateTextFile(dir2innerfile, std::wstring());
  FilePath file1 = temp_dir_.path().Append(FPL("file1.txt"));
  CreateTextFile(file1, std::wstring());
  FilePath file2_rel = dir2.Append(FilePath::kParentDirectory)
      .Append(FPL("file2.txt"));
  CreateTextFile(file2_rel, std::wstring());
  FilePath file2_abs = temp_dir_.path().Append(FPL("file2.txt"));

  // Only enumerate files.
  FileEnumerator f1(temp_dir_.path(), true, FileEnumerator::FILES);
  FindResultCollector c1(&f1);
  EXPECT_TRUE(c1.HasFile(file1));
  EXPECT_TRUE(c1.HasFile(file2_abs));
  EXPECT_TRUE(c1.HasFile(dir2file));
  EXPECT_TRUE(c1.HasFile(dir2innerfile));
  EXPECT_EQ(4, c1.size());

  // Only enumerate directories.
  FileEnumerator f2(temp_dir_.path(), true, FileEnumerator::DIRECTORIES);
  FindResultCollector c2(&f2);
  EXPECT_TRUE(c2.HasFile(dir1));
  EXPECT_TRUE(c2.HasFile(dir2));
  EXPECT_TRUE(c2.HasFile(dir2inner));
  EXPECT_EQ(3, c2.size());

  // Only enumerate directories non-recursively.
  FileEnumerator f2_non_recursive(
      temp_dir_.path(), false, FileEnumerator::DIRECTORIES);
  FindResultCollector c2_non_recursive(&f2_non_recursive);
  EXPECT_TRUE(c2_non_recursive.HasFile(dir1));
  EXPECT_TRUE(c2_non_recursive.HasFile(dir2));
  EXPECT_EQ(2, c2_non_recursive.size());

  // Only enumerate directories, non-recursively, including "..".
  FileEnumerator f2_dotdot(temp_dir_.path(), false,
                           FileEnumerator::DIRECTORIES |
                           FileEnumerator::INCLUDE_DOT_DOT);
  FindResultCollector c2_dotdot(&f2_dotdot);
  EXPECT_TRUE(c2_dotdot.HasFile(dir1));
  EXPECT_TRUE(c2_dotdot.HasFile(dir2));
  EXPECT_TRUE(c2_dotdot.HasFile(temp_dir_.path().Append(FPL(".."))));
  EXPECT_EQ(3, c2_dotdot.size());

  // Enumerate files and directories.
  FileEnumerator f3(temp_dir_.path(), true, FILES_AND_DIRECTORIES);
  FindResultCollector c3(&f3);
  EXPECT_TRUE(c3.HasFile(dir1));
  EXPECT_TRUE(c3.HasFile(dir2));
  EXPECT_TRUE(c3.HasFile(file1));
  EXPECT_TRUE(c3.HasFile(file2_abs));
  EXPECT_TRUE(c3.HasFile(dir2file));
  EXPECT_TRUE(c3.HasFile(dir2inner));
  EXPECT_TRUE(c3.HasFile(dir2innerfile));
  EXPECT_EQ(7, c3.size());

  // Non-recursive operation.
  FileEnumerator f4(temp_dir_.path(), false, FILES_AND_DIRECTORIES);
  FindResultCollector c4(&f4);
  EXPECT_TRUE(c4.HasFile(dir2));
  EXPECT_TRUE(c4.HasFile(dir2));
  EXPECT_TRUE(c4.HasFile(file1));
  EXPECT_TRUE(c4.HasFile(file2_abs));
  EXPECT_EQ(4, c4.size());

  // Enumerate with a pattern.
  FileEnumerator f5(temp_dir_.path(), true, FILES_AND_DIRECTORIES, FPL("dir*"));
  FindResultCollector c5(&f5);
  EXPECT_TRUE(c5.HasFile(dir1));
  EXPECT_TRUE(c5.HasFile(dir2));
  EXPECT_TRUE(c5.HasFile(dir2file));
  EXPECT_TRUE(c5.HasFile(dir2inner));
  EXPECT_TRUE(c5.HasFile(dir2innerfile));
  EXPECT_EQ(5, c5.size());

  // Make sure the destructor closes the find handle while in the middle of a
  // query to allow TearDown to delete the directory.
  FileEnumerator f9(temp_dir_.path(), true, FILES_AND_DIRECTORIES);
  EXPECT_FALSE(f9.Next().value().empty());  // Should have found something
                                            // (we don't care what).
}

TEST_F(FileUtilTest, AppendToFile) {
  FilePath data_dir =
      temp_dir_.path().Append(FILE_PATH_LITERAL("FilePathTest"));

  // Create a fresh, empty copy of this directory.
  if (PathExists(data_dir)) {
    ASSERT_TRUE(DeleteFile(data_dir, true));
  }
  ASSERT_TRUE(CreateDirectory(data_dir));

  // Create a fresh, empty copy of this directory.
  if (PathExists(data_dir)) {
    ASSERT_TRUE(DeleteFile(data_dir, true));
  }
  ASSERT_TRUE(CreateDirectory(data_dir));
  FilePath foobar(data_dir.Append(FILE_PATH_LITERAL("foobar.txt")));

  std::string data("hello");
  EXPECT_FALSE(AppendToFile(foobar, data.c_str(), data.size()));
  EXPECT_EQ(static_cast<int>(data.length()),
            WriteFile(foobar, data.c_str(), data.length()));
  EXPECT_TRUE(AppendToFile(foobar, data.c_str(), data.size()));

  const std::wstring read_content = ReadTextFile(foobar);
  EXPECT_EQ(L"hellohello", read_content);
}

TEST_F(FileUtilTest, ReadFile) {
  // Create a test file to be read.
  const std::string kTestData("The quick brown fox jumps over the lazy dog.");
  FilePath file_path =
      temp_dir_.path().Append(FILE_PATH_LITERAL("ReadFileTest"));

  ASSERT_EQ(static_cast<int>(kTestData.size()),
            WriteFile(file_path, kTestData.data(), kTestData.size()));

  // Make buffers with various size.
  std::vector<char> small_buffer(kTestData.size() / 2);
  std::vector<char> exact_buffer(kTestData.size());
  std::vector<char> large_buffer(kTestData.size() * 2);

  // Read the file with smaller buffer.
  int bytes_read_small = ReadFile(
      file_path, &small_buffer[0], static_cast<int>(small_buffer.size()));
  EXPECT_EQ(static_cast<int>(small_buffer.size()), bytes_read_small);
  EXPECT_EQ(
      std::string(kTestData.begin(), kTestData.begin() + small_buffer.size()),
      std::string(small_buffer.begin(), small_buffer.end()));

  // Read the file with buffer which have exactly same size.
  int bytes_read_exact = ReadFile(
      file_path, &exact_buffer[0], static_cast<int>(exact_buffer.size()));
  EXPECT_EQ(static_cast<int>(kTestData.size()), bytes_read_exact);
  EXPECT_EQ(kTestData, std::string(exact_buffer.begin(), exact_buffer.end()));

  // Read the file with larger buffer.
  int bytes_read_large = ReadFile(
      file_path, &large_buffer[0], static_cast<int>(large_buffer.size()));
  EXPECT_EQ(static_cast<int>(kTestData.size()), bytes_read_large);
  EXPECT_EQ(kTestData, std::string(large_buffer.begin(),
                                   large_buffer.begin() + kTestData.size()));

  // Make sure the return value is -1 if the file doesn't exist.
  FilePath file_path_not_exist =
      temp_dir_.path().Append(FILE_PATH_LITERAL("ReadFileNotExistTest"));
  EXPECT_EQ(-1,
            ReadFile(file_path_not_exist,
                     &exact_buffer[0],
                     static_cast<int>(exact_buffer.size())));
}

TEST_F(FileUtilTest, ReadFileToString) {
  const char kTestData[] = "0123";
  std::string data;

  FilePath file_path =
      temp_dir_.path().Append(FILE_PATH_LITERAL("ReadFileToStringTest"));
  FilePath file_path_dangerous =
      temp_dir_.path().Append(FILE_PATH_LITERAL("..")).
      Append(temp_dir_.path().BaseName()).
      Append(FILE_PATH_LITERAL("ReadFileToStringTest"));

  // Create test file.
  ASSERT_EQ(4, WriteFile(file_path, kTestData, 4));

  EXPECT_TRUE(ReadFileToString(file_path, &data));
  EXPECT_EQ(kTestData, data);

  data = "temp";
  EXPECT_FALSE(ReadFileToString(file_path, &data, 0));
  EXPECT_EQ(0u, data.length());

  data = "temp";
  EXPECT_FALSE(ReadFileToString(file_path, &data, 2));
  EXPECT_EQ("01", data);

  data.clear();
  EXPECT_FALSE(ReadFileToString(file_path, &data, 3));
  EXPECT_EQ("012", data);

  data.clear();
  EXPECT_TRUE(ReadFileToString(file_path, &data, 4));
  EXPECT_EQ("0123", data);

  data.clear();
  EXPECT_TRUE(ReadFileToString(file_path, &data, 6));
  EXPECT_EQ("0123", data);

  EXPECT_TRUE(ReadFileToString(file_path, NULL, 6));

  EXPECT_TRUE(ReadFileToString(file_path, NULL));

  data = "temp";
  EXPECT_FALSE(ReadFileToString(file_path_dangerous, &data));
  EXPECT_EQ(0u, data.length());

  // Delete test file.
  EXPECT_TRUE(DeleteFile(file_path, false));

  data = "temp";
  EXPECT_FALSE(ReadFileToString(file_path, &data));
  EXPECT_EQ(0u, data.length());

  data = "temp";
  EXPECT_FALSE(ReadFileToString(file_path, &data, 6));
  EXPECT_EQ(0u, data.length());
}

TEST_F(FileUtilTest, IsDirectoryEmpty) {
  FilePath empty_dir = temp_dir_.path().Append(FILE_PATH_LITERAL("EmptyDir"));

  ASSERT_FALSE(PathExists(empty_dir));

  ASSERT_TRUE(CreateDirectory(empty_dir));

  EXPECT_TRUE(IsDirectoryEmpty(empty_dir));

  FilePath foo(empty_dir.Append(FILE_PATH_LITERAL("foo.txt")));
  std::string bar("baz");
  ASSERT_TRUE(WriteFile(foo, bar.c_str(), bar.length()));

  EXPECT_FALSE(IsDirectoryEmpty(empty_dir));
}

}  // namespace

}  // namespace files
