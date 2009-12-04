// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/filesystem_iterator.h"
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <set>
#include <string>
#include <vector>
#include "chromeos/obsolete_logging.h"
#include "update_engine/utils.h"

using std::set;
using std::string;
using std::vector;

namespace chromeos_update_engine {

// We use a macro here for two reasons:
// 1. We want to be able to return from the caller's function.
// 2. We can use the #macro_arg ism to get a string of the calling statement,
//    which we can log.

#define RETURN_ERROR_IF_FALSE(_statement)                               \
  do {                                                                  \
    bool _result = (_statement);                                        \
    if (!_result) {                                                     \
      string _message = utils::ErrnoNumberAsString(errno);              \
      LOG(INFO) << #_statement << " failed: " << _message << ". Aborting"; \
      is_end_ = true;                                                   \
      is_err_ = true;                                                   \
      return;                                                           \
    }                                                                   \
  } while (0)

FilesystemIterator::FilesystemIterator(
    const std::string& path,
    const std::set<std::string>& excl_prefixes)
    : excl_prefixes_(excl_prefixes),
      is_end_(false),
      is_err_(false) {
  root_path_ = utils::NormalizePath(path, true);
  RETURN_ERROR_IF_FALSE(lstat(root_path_.c_str(), &stbuf_) == 0);
  root_dev_ = stbuf_.st_dev;
}

FilesystemIterator::~FilesystemIterator() {
  for (vector<DIR*>::iterator it = dirs_.begin(); it != dirs_.end(); ++it) {
    LOG_IF(ERROR, closedir(*it) != 0) << "closedir failed";
  }
}

// Returns full path for current file
std::string FilesystemIterator::GetFullPath() const {
  return root_path_ + GetPartialPath();
}

std::string FilesystemIterator::GetPartialPath() const {
  std::string ret;
  for (vector<string>::const_iterator it = names_.begin();
       it != names_.end(); ++it) {
    ret += "/";
    ret += *it;
  }
  return ret;
}

// Increments to the next file
void FilesystemIterator::Increment() {
  // If we're currently on a dir, descend into children, but only if
  // we're on the same device as the root device

  bool entering_dir = false;  // true if we're entering into a new dir
  if (S_ISDIR(stbuf_.st_mode) && (stbuf_.st_dev == root_dev_)) {
    DIR* dir = opendir(GetFullPath().c_str());
    if ((!dir) && ((errno == ENOTDIR) || (errno == ENOENT))) {
      // opendir failed b/c either it's not a dir or it doesn't exist.
      // that's fine. let's just skip over this.
      LOG(ERROR) << "Can't descend into " << GetFullPath();
    } else {
      RETURN_ERROR_IF_FALSE(dir);
      entering_dir = true;
      dirs_.push_back(dir);
    }
  }

  if (!entering_dir && names_.empty()) {
    // root disappeared while we tried to descend into it
    is_end_ = true;
    return;
  }

  if (!entering_dir)
    names_.pop_back();

  IncrementInternal();
  for (set<string>::const_iterator it = excl_prefixes_.begin();
       it != excl_prefixes_.end(); ++it) {
    if (utils::StringHasPrefix(GetPartialPath(), *it)) {
      Increment();
      break;
    }
  }
  return;
}

// Assumes that we need to find the next child of dirs_.back(), or if
// there are none more, go up the chain
void FilesystemIterator::IncrementInternal() {
  CHECK_EQ(dirs_.size(), names_.size() + 1);
  for (;;) {
    struct dirent dir_entry;
    struct dirent* dir_entry_pointer;
    int r;
    RETURN_ERROR_IF_FALSE(
        (r = readdir_r(dirs_.back(), &dir_entry, &dir_entry_pointer)) == 0);
    if (dir_entry_pointer) {
      // Found an entry
      names_.push_back(dir_entry_pointer->d_name);
      // Validate
      RETURN_ERROR_IF_FALSE(lstat(GetFullPath().c_str(), &stbuf_) == 0);
      if (strcmp(dir_entry_pointer->d_name, ".") &&
          strcmp(dir_entry_pointer->d_name, "..")) {
        // Done
        return;
      }
      // Child didn't work out. Try again
      names_.pop_back();
    } else {
      // No more children in this dir. Pop it and try again
      RETURN_ERROR_IF_FALSE(closedir(dirs_.back()) == 0);
      dirs_.pop_back();
      if (dirs_.empty()) {
        CHECK(names_.empty());
        // Done with the entire iteration
        is_end_ = true;
        return;
      }
      CHECK(!names_.empty());
      names_.pop_back();
    }
  }
}

}  //   namespace chromeos_update_engine
