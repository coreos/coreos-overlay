// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "files/file_util.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/basictypes.h"
#include <glog/logging.h>

#include "files/file_enumerator.h"
#include "files/file_path.h"
#include "files/scoped_file.h"
#include "files/eintr_wrapper.h"

namespace files {

namespace {

typedef struct stat stat_wrapper_t;
static int CallStat(const char *path, stat_wrapper_t *sb) {
  return stat(path, sb);
}
static int CallLstat(const char *path, stat_wrapper_t *sb) {
  return lstat(path, sb);
}

std::string TempFileName() {
  return std::string(".org.chromium.Chromium.XXXXXX");
}

// Creates and opens a temporary file in |directory|, returning the
// file descriptor. |path| is set to the temporary file path.
// This function does NOT unlink() the file.
int CreateAndOpenFdForTemporaryFile(FilePath directory, FilePath* path) {
  *path = directory.Append(TempFileName());
  const std::string& tmpdir_string = path->value();
  // this should be OK since mkstemp just replaces characters in place
  char* buffer = const_cast<char*>(tmpdir_string.c_str());

  return HANDLE_EINTR(mkstemp(buffer));
}

}  // namespace

FilePath MakeAbsoluteFilePath(const FilePath& input) {
  char full_path[PATH_MAX];
  if (realpath(input.value().c_str(), full_path) == NULL)
    return FilePath();
  return FilePath(full_path);
}

// TODO(erikkay): The Windows version of this accepts paths like "foo/bar/*"
// which works both with and without the recursive flag.  I'm not sure we need
// that functionality. If not, remove from file_util_win.cc, otherwise add it
// here.
bool DeleteFile(const FilePath& path, bool recursive) {
  const char* path_str = path.value().c_str();
  stat_wrapper_t file_info;
  int test = CallLstat(path_str, &file_info);
  if (test != 0) {
    // The Windows version defines this condition as success.
    bool ret = (errno == ENOENT || errno == ENOTDIR);
    return ret;
  }
  if (!S_ISDIR(file_info.st_mode))
    return (unlink(path_str) == 0);
  if (!recursive)
    return (rmdir(path_str) == 0);

  bool success = true;
  std::stack<std::string> directories;
  directories.push(path.value());
  FileEnumerator traversal(path, true,
      FileEnumerator::FILES | FileEnumerator::DIRECTORIES |
      FileEnumerator::SHOW_SYM_LINKS);
  for (FilePath current = traversal.Next(); success && !current.empty();
       current = traversal.Next()) {
    if (traversal.GetInfo().IsDirectory())
      directories.push(current.value());
    else
      success = (unlink(current.value().c_str()) == 0);
  }

  while (success && !directories.empty()) {
    FilePath dir = FilePath(directories.top());
    directories.pop();
    success = (rmdir(dir.value().c_str()) == 0);
  }
  return success;
}

bool ReplaceFile(const FilePath& from_path, const FilePath& to_path) {
  if (rename(from_path.value().c_str(), to_path.value().c_str()) == 0)
    return true;
  return false;
}

bool CopyDirectory(const FilePath& from_path,
                   const FilePath& to_path,
                   bool recursive) {
  // Some old callers of CopyDirectory want it to support wildcards.
  // After some discussion, we decided to fix those callers.
  // Break loudly here if anyone tries to do this.
  DCHECK(to_path.value().find('*') == std::string::npos);
  DCHECK(from_path.value().find('*') == std::string::npos);

  if (from_path.value().size() >= PATH_MAX) {
    return false;
  }

  // This function does not properly handle destinations within the source
  FilePath real_to_path = to_path;
  if (PathExists(real_to_path)) {
    real_to_path = MakeAbsoluteFilePath(real_to_path);
    if (real_to_path.empty())
      return false;
  } else {
    real_to_path = MakeAbsoluteFilePath(real_to_path.DirName());
    if (real_to_path.empty())
      return false;
  }
  FilePath real_from_path = MakeAbsoluteFilePath(from_path);
  if (real_from_path.empty())
    return false;
  if (real_to_path.value().size() >= real_from_path.value().size() &&
      real_to_path.value().compare(0, real_from_path.value().size(),
                                   real_from_path.value()) == 0) {
    return false;
  }

  int traverse_type = FileEnumerator::FILES | FileEnumerator::SHOW_SYM_LINKS;
  if (recursive)
    traverse_type |= FileEnumerator::DIRECTORIES;
  FileEnumerator traversal(from_path, recursive, traverse_type);

  // We have to mimic windows behavior here. |to_path| may not exist yet,
  // start the loop with |to_path|.
  struct stat from_stat;
  FilePath current = from_path;
  if (stat(from_path.value().c_str(), &from_stat) < 0) {
    DLOG(ERROR) << "CopyDirectory() couldn't stat source directory: "
                << from_path.value() << " errno = " << errno;
    return false;
  }
  struct stat to_path_stat;
  FilePath from_path_base = from_path;
  if (recursive && stat(to_path.value().c_str(), &to_path_stat) == 0 &&
      S_ISDIR(to_path_stat.st_mode)) {
    // If the destination already exists and is a directory, then the
    // top level of source needs to be copied.
    from_path_base = from_path.DirName();
  }

  // The Windows version of this function assumes that non-recursive calls
  // will always have a directory for from_path.
  // TODO(maruel): This is not necessary anymore.
  DCHECK(recursive || S_ISDIR(from_stat.st_mode));

  bool success = true;
  while (success && !current.empty()) {
    // current is the source path, including from_path, so append
    // the suffix after from_path to to_path to create the target_path.
    FilePath target_path(to_path);
    if (from_path_base != current) {
      if (!from_path_base.AppendRelativePath(current, &target_path)) {
        success = false;
        break;
      }
    }

    if (S_ISDIR(from_stat.st_mode)) {
      if (mkdir(target_path.value().c_str(),
                (from_stat.st_mode & 01777) | S_IRUSR | S_IXUSR | S_IWUSR) !=
              0 &&
          errno != EEXIST) {
        DLOG(ERROR) << "CopyDirectory() couldn't create directory: "
                    << target_path.value() << " errno = " << errno;
        success = false;
      }
    } else if (S_ISREG(from_stat.st_mode)) {
      if (!CopyFile(current, target_path)) {
        DLOG(ERROR) << "CopyDirectory() couldn't create file: "
                    << target_path.value();
        success = false;
      }
    } else {
      DLOG(WARNING) << "CopyDirectory() skipping non-regular file: "
                    << current.value();
    }

    current = traversal.Next();
    if (!current.empty())
      from_stat = traversal.GetInfo().stat();
  }

  return success;
}

bool PathExists(const FilePath& path) {
  return access(path.value().c_str(), F_OK) == 0;
}

bool PathIsWritable(const FilePath& path) {
  return access(path.value().c_str(), W_OK) == 0;
}

bool DirectoryExists(const FilePath& path) {
  stat_wrapper_t file_info;
  if (CallStat(path.value().c_str(), &file_info) == 0)
    return S_ISDIR(file_info.st_mode);
  return false;
}

bool ReadFromFD(int fd, char* buffer, size_t bytes) {
  size_t total_read = 0;
  while (total_read < bytes) {
    ssize_t bytes_read =
        HANDLE_EINTR(read(fd, buffer + total_read, bytes - total_read));
    if (bytes_read <= 0)
      break;
    total_read += bytes_read;
  }
  return total_read == bytes;
}

bool CreateSymbolicLink(const FilePath& target_path,
                        const FilePath& symlink_path) {
  DCHECK(!symlink_path.empty());
  DCHECK(!target_path.empty());
  return ::symlink(target_path.value().c_str(),
                   symlink_path.value().c_str()) != -1;
}

bool ReadSymbolicLink(const FilePath& symlink_path, FilePath* target_path) {
  DCHECK(!symlink_path.empty());
  DCHECK(target_path);
  char buf[PATH_MAX];
  ssize_t count = ::readlink(symlink_path.value().c_str(), buf, arraysize(buf));

  if (count <= 0) {
    target_path->clear();
    return false;
  }

  *target_path = FilePath(FilePath::StringType(buf, count));
  return true;
}

bool GetPosixFilePermissions(const FilePath& path, int* mode) {
  DCHECK(mode);

  stat_wrapper_t file_info;
  // Uses stat(), because on symbolic link, lstat() does not return valid
  // permission bits in st_mode
  if (CallStat(path.value().c_str(), &file_info) != 0)
    return false;

  *mode = file_info.st_mode & FILE_PERMISSION_MASK;
  return true;
}

bool SetPosixFilePermissions(const FilePath& path,
                             int mode) {
  DCHECK_EQ(mode & ~FILE_PERMISSION_MASK, 0);

  // Calls stat() so that we can preserve the higher bits like S_ISGID.
  stat_wrapper_t stat_buf;
  if (CallStat(path.value().c_str(), &stat_buf) != 0)
    return false;

  // Clears the existing permission bits, and adds the new ones.
  mode_t updated_mode_bits = stat_buf.st_mode & ~FILE_PERMISSION_MASK;
  updated_mode_bits |= mode & FILE_PERMISSION_MASK;

  if (HANDLE_EINTR(chmod(path.value().c_str(), updated_mode_bits)) != 0)
    return false;

  return true;
}

bool GetTempDir(FilePath* path) {
  const char* tmp = getenv("TMPDIR");
  if (tmp) {
    *path = FilePath(tmp);
  } else {
    *path = FilePath("/tmp");
  }
  return true;
}

bool CreateTemporaryFile(FilePath* path) {
  FilePath directory;
  if (!GetTempDir(&directory))
    return false;
  int fd = CreateAndOpenFdForTemporaryFile(directory, path);
  if (fd < 0)
    return false;
  close(fd);
  return true;
}

FILE* CreateAndOpenTemporaryFileInDir(const FilePath& dir, FilePath* path) {
  int fd = CreateAndOpenFdForTemporaryFile(dir, path);
  if (fd < 0)
    return NULL;

  FILE* file = fdopen(fd, "a+");
  if (!file)
    close(fd);
  return file;
}

bool CreateTemporaryFileInDir(const FilePath& dir, FilePath* temp_file) {
  int fd = CreateAndOpenFdForTemporaryFile(dir, temp_file);
  return ((fd >= 0) && !IGNORE_EINTR(close(fd)));
}

static bool CreateTemporaryDirInDirImpl(const FilePath& base_dir,
                                        const FilePath::StringType& name_tmpl,
                                        FilePath* new_dir) {
  DCHECK(name_tmpl.find("XXXXXX") != FilePath::StringType::npos)
      << "Directory name template must contain \"XXXXXX\".";

  FilePath sub_dir = base_dir.Append(name_tmpl);
  std::string sub_dir_string = sub_dir.value();

  // this should be OK since mkdtemp just replaces characters in place
  char* buffer = const_cast<char*>(sub_dir_string.c_str());
  char* dtemp = mkdtemp(buffer);
  if (!dtemp) {
    PLOG(ERROR) << "mkdtemp failed";
    return false;
  }
  *new_dir = FilePath(dtemp);
  return true;
}

bool CreateTemporaryDirInDir(const FilePath& base_dir,
                             const FilePath::StringType& prefix,
                             FilePath* new_dir) {
  FilePath::StringType mkdtemp_template = prefix;
  mkdtemp_template.append(FILE_PATH_LITERAL("XXXXXX"));
  return CreateTemporaryDirInDirImpl(base_dir, mkdtemp_template, new_dir);
}

bool CreateNewTempDirectory(const FilePath::StringType& prefix,
                            FilePath* new_temp_path) {
  FilePath tmpdir;
  if (!GetTempDir(&tmpdir))
    return false;

  return CreateTemporaryDirInDirImpl(tmpdir, TempFileName(), new_temp_path);
}

bool CreateDirectory(const FilePath& full_path) {
  std::vector<FilePath> subpaths;

  // Collect a list of all parent directories.
  FilePath last_path = full_path;
  subpaths.push_back(full_path);
  for (FilePath path = full_path.DirName();
       path.value() != last_path.value(); path = path.DirName()) {
    subpaths.push_back(path);
    last_path = path;
  }

  // Iterate through the parents and create the missing ones.
  for (std::vector<FilePath>::reverse_iterator i = subpaths.rbegin();
       i != subpaths.rend(); ++i) {
    if (DirectoryExists(*i))
      continue;
    if (mkdir(i->value().c_str(), 0700) == 0)
      continue;
    // Mkdir failed, but it might have failed with EEXIST, or some other error
    // due to the the directory appearing out of thin air. This can occur if
    // two processes are trying to create the same file system tree at the same
    // time. Check to see if it exists and make sure it is a directory.
    if (!DirectoryExists(*i)) {
      return false;
    }
  }
  return true;
}

// TODO(rkc): Refactor GetFileInfo and FileEnumerator to handle symlinks
// correctly. http://code.google.com/p/chromium-os/issues/detail?id=15948
bool IsLink(const FilePath& file_path) {
  stat_wrapper_t st;
  // If we can't lstat the file, it's safe to assume that the file won't at
  // least be a 'followable' link.
  if (CallLstat(file_path.value().c_str(), &st) != 0)
    return false;

  if (S_ISLNK(st.st_mode))
    return true;
  else
    return false;
}

FILE* OpenFile(const FilePath& filename, const char* mode) {
  FILE* result = NULL;
  do {
    result = fopen(filename.value().c_str(), mode);
  } while (!result && errno == EINTR);
  return result;
}

int ReadFile(const FilePath& filename, char* data, int max_size) {
  int fd = HANDLE_EINTR(open(filename.value().c_str(), O_RDONLY));
  if (fd < 0)
    return -1;

  ssize_t bytes_read = HANDLE_EINTR(read(fd, data, max_size));
  if (IGNORE_EINTR(close(fd)) < 0)
    return -1;
  return bytes_read;
}

int WriteFile(const FilePath& filename, const char* data, int size) {
  int fd = HANDLE_EINTR(creat(filename.value().c_str(), 0640));
  if (fd < 0)
    return -1;

  int bytes_written = WriteFileDescriptor(fd, data, size) ? size : -1;
  if (IGNORE_EINTR(close(fd)) < 0)
    return -1;
  return bytes_written;
}

bool WriteFileDescriptor(const int fd, const char* data, int size) {
  // Allow for partial writes.
  ssize_t bytes_written_total = 0;
  for (ssize_t bytes_written_partial = 0; bytes_written_total < size;
       bytes_written_total += bytes_written_partial) {
    bytes_written_partial =
        HANDLE_EINTR(write(fd, data + bytes_written_total,
                           size - bytes_written_total));
    if (bytes_written_partial < 0)
      return false;
  }

  return true;
}

bool AppendToFile(const FilePath& filename, const char* data, int size) {
  bool ret = true;
  int fd = HANDLE_EINTR(open(filename.value().c_str(), O_WRONLY | O_APPEND));
  if (fd < 0) {
    PLOG(WARNING) << "Unable to create file " << filename.value();
    return false;
  }

  // This call will either write all of the data or return false.
  if (!WriteFileDescriptor(fd, data, size)) {
    PLOG(WARNING) << "Error while writing to file " << filename.value();
    ret = false;
  }

  if (IGNORE_EINTR(close(fd)) < 0) {
    PLOG(WARNING) << "Error while closing file " << filename.value();
    return false;
  }

  return ret;
}

bool CopyFile(const FilePath& from_path, const FilePath& to_path) {
  if (to_path.ReferencesParent())
    return false;

  ScopedFD infile(HANDLE_EINTR(
      open(from_path.value().c_str(), O_RDONLY)));
  if (!infile.is_valid())
    return false;

  ScopedFD outfile(HANDLE_EINTR(
      open(to_path.value().c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0600)));
  if (!outfile.is_valid())
    return false;

  const size_t kBufferSize = 32768;
  std::vector<char> buffer(kBufferSize);

  while (true) {
    ssize_t bytes_read = HANDLE_EINTR(
        read(infile.get(), &buffer[0], buffer.size()));
    if (bytes_read < 0)
      return false;
    if (bytes_read == 0)
      break;
    if (!WriteFileDescriptor(outfile.get(), &buffer[0], bytes_read))
      return false;
  }

  return true;
}

// -----------------------------------------------------------------------------

namespace internal {

bool MoveUnsafe(const FilePath& from_path, const FilePath& to_path) {
  // Windows compatibility: if to_path exists, from_path and to_path
  // must be the same type, either both files, or both directories.
  stat_wrapper_t to_file_info;
  if (CallStat(to_path.value().c_str(), &to_file_info) == 0) {
    stat_wrapper_t from_file_info;
    if (CallStat(from_path.value().c_str(), &from_file_info) == 0) {
      if (S_ISDIR(to_file_info.st_mode) != S_ISDIR(from_file_info.st_mode))
        return false;
    } else {
      return false;
    }
  }

  if (rename(from_path.value().c_str(), to_path.value().c_str()) == 0)
    return true;

  if (!CopyDirectory(from_path, to_path, true))
    return false;

  DeleteFile(from_path, true);
  return true;
}

}  // namespace internal

}  // namespace files
