// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UTILS_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UTILS_H__

#include <set>
#include <string>
#include <vector>
#include "update_engine/action.h"
#include "update_engine/action_processor.h"

namespace chromeos_update_engine {

namespace utils {

// Returns the entire contents of the file at path. Returns true on success.
bool ReadFile(const std::string& path, std::vector<char>* out);
bool ReadFileToString(const std::string& path, std::string* out);

std::string ErrnoNumberAsString(int err);

// Strips duplicate slashes, and optionally removes all trailing slashes.
// Does not compact /./ or /../.
std::string NormalizePath(const std::string& path, bool strip_trailing_slash);

// Returns true if the file exists for sure. Returns false if it doesn't exist,
// or an error occurs.
bool FileExists(const char* path);

// The last 6 chars of path must be XXXXXX. They will be randomly changed
// and a non-existent path will be returned. Intentionally makes a copy
// of the string passed in.
// NEVER CALL THIS FUNCTION UNLESS YOU ARE SURE
// THAT YOUR PROCESS WILL BE THE ONLY THING WRITING FILES IN THIS DIRECTORY.
std::string TempFilename(string path);

// Deletes a directory and all its contents synchronously. Returns true
// on success. This may be called with a regular file--it will just unlink it.
// This WILL cross filesystem boundaries.
bool RecursiveUnlinkDir(const std::string& path);

// Synchronously mount or unmount a filesystem. Return true on success.
// Mounts as ext3 with default options.
bool MountFilesystem(const string& device, const string& mountpoint);
bool UnmountFilesystem(const string& mountpoint);

// Log a string in hex to LOG(INFO). Useful for debugging.
void HexDumpArray(const unsigned char* const arr, const size_t length);
inline void HexDumpString(const std::string& str) {
  HexDumpArray(reinterpret_cast<const unsigned char*>(str.data()), str.size());
}
inline void HexDumpVector(const std::vector<char>& vect) {
  HexDumpArray(reinterpret_cast<const unsigned char*>(&vect[0]), vect.size());
}

extern const string kStatefulPartition;

bool StringHasSuffix(const std::string& str, const std::string& suffix);
bool StringHasPrefix(const std::string& str, const std::string& prefix);

template<typename KeyType, typename ValueType>
bool MapContainsKey(const std::map<KeyType, ValueType>& m, const KeyType& k) {
  return m.find(k) != m.end();
}

template<typename ValueType>
std::set<ValueType> SetWithValue(const ValueType& value) {
  std::set<ValueType> ret;
  ret.insert(value);
  return ret;
}

// Returns the currently booted device. "/dev/sda1", for example.
// This will not interpret LABEL= or UUID=. You'll need to use findfs
// or something with equivalent funcionality to interpret those.
const std::string BootDevice();

}  // namespace utils

// Class to unmount FS when object goes out of scope
class ScopedFilesystemUnmounter {
 public:
  explicit ScopedFilesystemUnmounter(const std::string& mountpoint)
      : mountpoint_(mountpoint) {}
  ~ScopedFilesystemUnmounter() {
    utils::UnmountFilesystem(mountpoint_);
  }
 private:
  const std::string mountpoint_;
};

// Utility class to close a file descriptor
class ScopedFdCloser {
 public:
  explicit ScopedFdCloser(int* fd) : fd_(fd), should_close_(true) {}
  void set_should_close(bool should_close) { should_close_ = should_close; }
  ~ScopedFdCloser() {
    if (!should_close_)
      return;
    if (fd_ && (*fd_ >= 0)) {
      close(*fd_);
      *fd_ = -1;
    }
  }
 private:
  int* fd_;
  bool should_close_;
};

// A little object to call ActionComplete on the ActionProcessor when
// it's destructed.
class ScopedActionCompleter {
 public:
  explicit ScopedActionCompleter(ActionProcessor* processor,
                                 AbstractAction* action)
      : processor_(processor),
        action_(action),
        success_(false),
        should_complete_(true) {}
  ~ScopedActionCompleter() {
    if (should_complete_)
      processor_->ActionComplete(action_, success_);
  }
  void set_success(bool success) {
    success_ = success;
  }
  void set_should_complete(bool should_complete) {
    should_complete_ = should_complete;
  }
 private:
  ActionProcessor* processor_;
  AbstractAction* action_;
  bool success_;
  bool should_complete_;
  DISALLOW_COPY_AND_ASSIGN(ScopedActionCompleter);
};

}  // namespace chromeos_update_engine

#define TEST_AND_RETURN_FALSE_ERRNO(_x)                                        \
  do {                                                                         \
    bool _success = (_x);                                                      \
    if (!_success) {                                                           \
      std::string _msg =                                                       \
          chromeos_update_engine::utils::ErrnoNumberAsString(errno);           \
      LOG(ERROR) << #_x " failed: " << _msg;                                   \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define TEST_AND_RETURN_FALSE(_x)                                              \
  do {                                                                         \
    bool _success = (_x);                                                      \
    if (!_success) {                                                           \
      LOG(ERROR) << #_x " failed.";                                            \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define TEST_AND_RETURN_ERRNO(_x)                                              \
  do {                                                                         \
    bool _success = (_x);                                                      \
    if (!_success) {                                                           \
      std::string _msg =                                                       \
          chromeos_update_engine::utils::ErrnoNumberAsString(errno);           \
      LOG(ERROR) << #_x " failed: " << _msg;                                   \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define TEST_AND_RETURN(_x)                                                    \
  do {                                                                         \
    bool _success = (_x);                                                      \
    if (!_success) {                                                           \
      LOG(ERROR) << #_x " failed.";                                            \
      return;                                                                  \
    }                                                                          \
  } while (0)



#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UTILS_H__
