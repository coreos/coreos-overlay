// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_FILESYSTEM_ITERATOR_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_FILESYSTEM_ITERATOR_H__

// This class is used to walk a filesystem. It will iterate over every file
// on the same device as the file passed in the ctor. Directories will be
// visited before their children. Children will be visited in no particular
// order.

// The iterator is a forward iterator. It's not random access nor can it be
// decremented.

// Note: If the iterator comes across a mount point where another filesystem
// is mounted, that mount point will be present, but none of its children
// will be. Technically the mount point is on the other filesystem (and
// the Stat() call will verify that), but we return it anyway since:
// 1. Such a folder must exist in the first filesystem if it got used
//    as a mount point.
// 2. You probably want to copy if it you're using the iterator to do a
//    filesystem copy
// 3. If you don't want that, you can just check Stat().st_dev and skip
//    foreign filesystems manually.

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <string>
#include <set>
#include <vector>

namespace chromeos_update_engine {

class FilesystemIterator {
 public:
  FilesystemIterator(const std::string& path,
                     const std::set<std::string>& excl_prefixes);

  ~FilesystemIterator();

  // Returns stat struct for the current file.
  struct stat GetStat() const {
    return stbuf_;
  }

  // Returns full path for current file.
  std::string GetFullPath() const;

  // Returns the path that's part of the iterator. For example, if
  // the object were constructed by passing in "/foo/bar" and Path()
  // returns "/foo/bar/baz/bat.txt", IterPath would return
  // "/baz/bat.txt". When this object is on root (ie, the very first
  // path), IterPath will return "", otherwise the first character of
  // IterPath will be "/".
  std::string GetPartialPath() const;

  // Returns name for current file.
  std::string GetBasename() const {
    return names_.back();
  }

  // Increments to the next file.
  void Increment();

  // If we're at the end. If at the end, do not call Stat(), Path(), etc.,
  // since this iterator currently isn't pointing to any file at all.
  bool IsEnd() const {
    return is_end_;
  }

  // Returns true if the iterator is in an error state.
  bool IsErr() const {
    return is_err_;
  }
 private:
  // Helper for Increment.
  void IncrementInternal();

  // Returns true if path exists and it's a directory.
  bool DirectoryExists(const std::string& path);

  // In general (i.e., not midway through a call to Increment()), there is a
  // relationship between dirs_ and names_: dirs[i] == names_[i - 1].
  // For example, say we are asked to iterate "/usr/local" and we're currently
  // at /usr/local/share/dict/words. dirs_ contains DIR* variables for the
  // dirs at: {"/usr/local", ".../share", ".../dict"} and names_ contains:
  // {"share", "dict", "words"}. root_path_ contains "/usr/local".
  // root_dev_ would be the dev for root_path_
  // (and /usr/local/share/dict/words). stbuf_ would be the stbuf for
  // /usr/local/share/dict/words.

  // All opened directories. If this is empty, we're currently on the root,
  // but not descended into the root.
  // This will always contain the current directory and all it's ancestors
  // in root-to-leaf order. For more details, see comment above.
  std::vector<DIR*> dirs_;

  // The list of all filenames for the current path that we've descended into.
  std::vector<std::string> names_;

  // The device of the root path we've been asked to iterate.
  dev_t root_dev_;

  // The root path we've been asked to iteratate.
  std::string root_path_;

  // Exclude items w/ this prefix.
  std::set<std::string> excl_prefixes_;

  // The struct stat of the current file we're at.
  struct stat stbuf_;

  // Generally false; set to true when we reach the end of files to iterate
  // or error occurs.
  bool is_end_;

  // Generally false; set to true if an error occurrs.
  bool is_err_;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_FILESYSTEM_ITERATOR_H__
