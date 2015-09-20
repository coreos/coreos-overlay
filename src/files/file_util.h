// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for dealing with the local
// filesystem.

#ifndef FILES_FILE_UTIL_H_
#define FILES_FILE_UTIL_H_

#include <sys/stat.h>
#include <stdio.h>

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "files/file_path.h"
#include "files/eintr_wrapper.h"

namespace files {

//-----------------------------------------------------------------------------
// Functions that involve filesystem access or modification:

// Returns an absolute version of a relative path. Returns an empty path on
// error. On POSIX, this function fails if the path does not exist. This
// function can result in I/O so it can be slow.
FilePath MakeAbsoluteFilePath(const FilePath& input);

// Deletes the given path, whether it's a file or a directory.
// If it's a directory, it's perfectly happy to delete all of the
// directory's contents.  Passing true to recursive deletes
// subdirectories and their contents as well.
// Returns true if successful, false otherwise. It is considered successful
// to attempt to delete a file that does not exist.
//
// In posix environment and if |path| is a symbolic link, this deletes only
// the symlink. (even if the symlink points to a non-existent file)
//
// WARNING: USING THIS WITH recursive==true IS EQUIVALENT
//          TO "rm -rf", SO USE WITH CAUTION.
bool DeleteFile(const FilePath& path, bool recursive);

// Moves the given path, whether it's a file or a directory.
// If a simple rename is not possible, such as in the case where the paths are
// on different volumes, this will attempt to copy and delete. Returns
// true for success.
// This function fails if either path contains traversal components ('..').
bool Move(const FilePath& from_path, const FilePath& to_path);

// Renames file |from_path| to |to_path|. Both paths must be on the same
// volume, or the function will fail. Destination file will be created
// if it doesn't exist. Prefer this function over Move when dealing with
// temporary files.
bool ReplaceFile(const FilePath& from_path, const FilePath& to_path);

// Copies a single file. Use CopyDirectory to copy directories.
// This function fails if either path contains traversal components ('..').
//
// This function keeps the metadata on Windows. The read only bit on Windows is
// not kept.
bool CopyFile(const FilePath& from_path, const FilePath& to_path);

// Copies the given path, and optionally all subdirectories and their contents
// as well.
//
// If there are files existing under to_path, always overwrite. Returns true
// if successful, false otherwise. Wildcards on the names are not supported.
//
// This function calls into CopyFile() so the same behavior w.r.t. metadata
// applies.
//
// If you only need to copy a file use CopyFile, it's faster.
bool CopyDirectory(const FilePath& from_path,
                               const FilePath& to_path,
                               bool recursive);

// Returns true if the given path exists on the local filesystem,
// false otherwise.
bool PathExists(const FilePath& path);

// Returns true if the given path is writable by the user, false otherwise.
bool PathIsWritable(const FilePath& path);

// Returns true if the given path exists and is a directory, false otherwise.
bool DirectoryExists(const FilePath& path);

// Reads the file at |path| into |contents| and returns true on success and
// false on error.  For security reasons, a |path| containing path traversal
// components ('..') is treated as a read error and |contents| is set to empty.
// In case of I/O error, |contents| holds the data that could be read from the
// file before the error occurred.
// |contents| may be NULL, in which case this function is useful for its side
// effect of priming the disk cache (could be used for unit tests).
bool ReadFileToString(const FilePath& path, std::string* contents);

// Reads the file at |path| into |contents| and returns true on success and
// false on error.  For security reasons, a |path| containing path traversal
// components ('..') is treated as a read error and |contents| is set to empty.
// In case of I/O error, |contents| holds the data that could be read from the
// file before the error occurred.  When the file size exceeds |max_size|, the
// function returns false with |contents| holding the file truncated to
// |max_size|.
// |contents| may be NULL, in which case this function is useful for its side
// effect of priming the disk cache (could be used for unit tests).
bool ReadFileToString(const FilePath& path,
                      std::string* contents,
                      size_t max_size);

// Read exactly |bytes| bytes from file descriptor |fd|, storing the result
// in |buffer|. This function is protected against EINTR and partial reads.
// Returns true iff |bytes| bytes have been successfully read from |fd|.
bool ReadFromFD(int fd, char* buffer, size_t bytes);

// Creates a symbolic link at |symlink| pointing to |target|.  Returns
// false on failure.
bool CreateSymbolicLink(const FilePath& target, const FilePath& symlink);

// Reads the given |symlink| and returns where it points to in |target|.
// Returns false upon failure.
bool ReadSymbolicLink(const FilePath& symlink, FilePath* target);

// Bits and masks of the file permission.
enum FilePermissionBits {
  FILE_PERMISSION_MASK              = S_IRWXU | S_IRWXG | S_IRWXO,
  FILE_PERMISSION_USER_MASK         = S_IRWXU,
  FILE_PERMISSION_GROUP_MASK        = S_IRWXG,
  FILE_PERMISSION_OTHERS_MASK       = S_IRWXO,

  FILE_PERMISSION_READ_BY_USER      = S_IRUSR,
  FILE_PERMISSION_WRITE_BY_USER     = S_IWUSR,
  FILE_PERMISSION_EXECUTE_BY_USER   = S_IXUSR,
  FILE_PERMISSION_READ_BY_GROUP     = S_IRGRP,
  FILE_PERMISSION_WRITE_BY_GROUP    = S_IWGRP,
  FILE_PERMISSION_EXECUTE_BY_GROUP  = S_IXGRP,
  FILE_PERMISSION_READ_BY_OTHERS    = S_IROTH,
  FILE_PERMISSION_WRITE_BY_OTHERS   = S_IWOTH,
  FILE_PERMISSION_EXECUTE_BY_OTHERS = S_IXOTH,
};

// Reads the permission of the given |path|, storing the file permission
// bits in |mode|. If |path| is symbolic link, |mode| is the permission of
// a file which the symlink points to.
bool GetPosixFilePermissions(const FilePath& path, int* mode);
// Sets the permission of the given |path|. If |path| is symbolic link, sets
// the permission of a file which the symlink points to.
bool SetPosixFilePermissions(const FilePath& path, int mode);

// Returns true if the given directory is empty
bool IsDirectoryEmpty(const FilePath& dir_path);

// Get the temporary directory provided by the system.
//
// WARNING: In general, you should use CreateTemporaryFile variants below
// instead of this function. Those variants will ensure that the proper
// permissions are set so that other users on the system can't edit them while
// they're open (which can lead to security issues).
bool GetTempDir(FilePath* path);

// Creates a temporary file. The full path is placed in |path|, and the
// function returns true if was successful in creating the file. The file will
// be empty and all handles closed after this function returns.
bool CreateTemporaryFile(FilePath* path);

// Same as CreateTemporaryFile but the file is created in |dir|.
bool CreateTemporaryFileInDir(const FilePath& dir, FilePath* temp_file);

// Create and open a temporary file.  File is opened for read/write.
// The full path is placed in |path|.
// Returns a handle to the opened file or NULL if an error occurred.
FILE* CreateAndOpenTemporaryFile(FilePath* path);

// Similar to CreateAndOpenTemporaryFile, but the file is created in |dir|.
FILE* CreateAndOpenTemporaryFileInDir(const FilePath& dir, FilePath* path);

// Create a new directory. If prefix is provided, the new directory name is in
// the format of prefixyyyy.
// NOTE: prefix is ignored in the POSIX implementation.
// If success, return true and output the full path of the directory created.
bool CreateNewTempDirectory(const FilePath::StringType& prefix,
                            FilePath* new_temp_path);

// Create a directory within another directory.
// Extra characters will be appended to |prefix| to ensure that the
// new directory does not have the same name as an existing directory.
bool CreateTemporaryDirInDir(const FilePath& base_dir,
                             const FilePath::StringType& prefix,
                             FilePath* new_dir);

// Creates a directory, as well as creating any parent directories, if they
// don't exist. Returns 'true' on successful creation, or if the directory
// already exists.  The directory is only readable by the current user.
bool CreateDirectory(const FilePath& full_path);

// Returns the file size. Returns true on success.
bool GetFileSize(const FilePath& file_path, int64* file_size);

// Sets |real_path| to |path| with symbolic links and junctions expanded.
// On windows, make sure the path starts with a lettered drive.
// |path| must reference a file.  Function will fail if |path| points to
// a directory or to a nonexistent path.  On windows, this function will
// fail if |path| is a junction or symlink that points to an empty file,
// or if |real_path| would be longer than MAX_PATH characters.
bool NormalizeFilePath(const FilePath& path, FilePath* real_path);

// This function will return if the given file is a symlink or not.
bool IsLink(const FilePath& file_path);

// Wrapper for fopen-like calls. Returns non-NULL FILE* on success.
FILE* OpenFile(const FilePath& filename, const char* mode);

// Closes file opened by OpenFile. Returns true on success.
bool CloseFile(FILE* file);

// Truncates an open file to end at the location of the current file pointer.
// This is a cross-platform analog to Windows' SetEndOfFile() function.
bool TruncateFile(FILE* file);

// Reads at most the given number of bytes from the file into the buffer.
// Returns the number of read bytes, or -1 on error.
int ReadFile(const FilePath& filename, char* data, int max_size);

// Writes the given buffer into the file, overwriting any data that was
// previously there.  Returns the number of bytes written, or -1 on error.
int WriteFile(const FilePath& filename, const char* data, int size);

// Appends |data| to |fd|. Does not close |fd| when done.  Returns true iff
// |size| bytes of |data| were written to |fd|.
bool WriteFileDescriptor(const int fd, const char* data, int size);

// Appends |data| to |filename|.  Returns true iff |size| bytes of |data| were
// written to |filename|.
bool AppendToFile(const FilePath& filename, const char* data, int size);

// Internal --------------------------------------------------------------------

namespace internal {

// Same as Move but allows paths with traversal components.
// Use only with extreme care.
bool MoveUnsafe(const FilePath& from_path, const FilePath& to_path);

}  // namespace internal
}  // namespace files

#endif  // FILES_FILE_UTIL_H_
