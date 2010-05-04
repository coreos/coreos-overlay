// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UTILS_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UTILS_H__

#include <errno.h>
#include <algorithm>
#include <set>
#include <string>
#include <vector>
#include <glib.h>
#include "update_engine/action.h"
#include "update_engine/action_processor.h"

namespace chromeos_update_engine {

namespace utils {

// Writes the data passed to path. The file at path will be overwritten if it
// exists. Returns true on success, false otherwise.
bool WriteFile(const char* path, const char* data, int data_len);

// Calls write() or pwrite() repeatedly until all count bytes at buf are
// written to fd or an error occurs. Returns true on success.
bool WriteAll(int fd, const void* buf, size_t count);
bool PWriteAll(int fd, const void* buf, size_t count, off_t offset);

// Calls pread() repeatedly until count bytes are read, or EOF is reached.
// Returns number of bytes read in *bytes_read. Returns true on success.
bool PReadAll(int fd, void* buf, size_t count, off_t offset,
              ssize_t* out_bytes_read);

// Returns the entire contents of the file at path. Returns true on success.
bool ReadFile(const std::string& path, std::vector<char>* out);
bool ReadFileToString(const std::string& path, std::string* out);

// Returns the size of the file at path. If the file doesn't exist or some
// error occurrs, -1 is returned.
off_t FileSize(const std::string& path);

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
std::string TempFilename(std::string path);

// Calls mkstemp() with the template passed. Returns the filename in the
// out param filename. If fd is non-NULL, the file fd returned by mkstemp
// is not close()d and is returned in the out param 'fd'. However, if
// fd is NULL, the fd from mkstemp() will be closed.
// The last six chars of the template must be XXXXXX.
// Returns true on success.
bool MakeTempFile(const std::string& filename_template,
                  std::string* filename,
                  int* fd);

// Calls mkdtemp() with the tempate passed. Returns the generated dirname
// in the dirname param. Returns TRUE on success. dirname must not be NULL.
bool MakeTempDirectory(const std::string& dirname_template,
                       std::string* dirname);

// Deletes a directory and all its contents synchronously. Returns true
// on success. This may be called with a regular file--it will just unlink it.
// This WILL cross filesystem boundaries.
bool RecursiveUnlinkDir(const std::string& path);

// Returns the root device for a partition. For example,
// RootDevice("/dev/sda3") returns "/dev/sda".
std::string RootDevice(const std::string& partition_device);

// Returns the partition number, as a string, of partition_device. For example,
// PartitionNumber("/dev/sda3") return "3".
std::string PartitionNumber(const std::string& partition_device);

// Synchronously mount or unmount a filesystem. Return true on success.
// Mounts as ext3 with default options.
bool MountFilesystem(const std::string& device, const std::string& mountpoint,
                     unsigned long flags);
bool UnmountFilesystem(const std::string& mountpoint);

enum BootLoader {
  BootLoader_SYSLINUX = 0,
  BootLoader_CHROME_FIRMWARE = 1
};
// Detects which bootloader this system uses and returns it via the out
// param. Returns true on success.
bool GetBootloader(BootLoader* out_bootloader);

// Returns the error message, if any, from a GError pointer.
const char* GetGErrorMessage(const GError* error);

// Log a string in hex to LOG(INFO). Useful for debugging.
void HexDumpArray(const unsigned char* const arr, const size_t length);
inline void HexDumpString(const std::string& str) {
  HexDumpArray(reinterpret_cast<const unsigned char*>(str.data()), str.size());
}
inline void HexDumpVector(const std::vector<char>& vect) {
  HexDumpArray(reinterpret_cast<const unsigned char*>(&vect[0]), vect.size());
}

extern const char* const kStatefulPartition;

bool StringHasSuffix(const std::string& str, const std::string& suffix);
bool StringHasPrefix(const std::string& str, const std::string& prefix);

template<typename KeyType, typename ValueType>
bool MapContainsKey(const std::map<KeyType, ValueType>& m, const KeyType& k) {
  return m.find(k) != m.end();
}
template<typename KeyType>
bool SetContainsKey(const std::set<KeyType>& s, const KeyType& k) {
  return s.find(k) != s.end();
}

template<typename ValueType>
std::set<ValueType> SetWithValue(const ValueType& value) {
  std::set<ValueType> ret;
  ret.insert(value);
  return ret;
}

template<typename T>
bool VectorContainsValue(const std::vector<T>& vect, const T& value) {
  return std::find(vect.begin(), vect.end(), value) != vect.end(); 
}

template<typename T>
bool VectorIndexOf(const std::vector<T>& vect, const T& value,
                   typename std::vector<T>::size_type* out_index) {
  typename std::vector<T>::const_iterator it = std::find(vect.begin(),
                                                         vect.end(),
                                                         value);
  if (it == vect.end()) {
    return false;
  } else {
    *out_index = it - vect.begin();
    return true;
  }
}

// Returns the currently booted device. "/dev/sda3", for example.
// This will not interpret LABEL= or UUID=. You'll need to use findfs
// or something with equivalent funcionality to interpret those.
const std::string BootDevice();

// Returns the currently booted kernel device, "dev/sda2", for example.
// Client must pass in the boot device. The suggested calling convention
// is: BootKernelDevice(BootDevice()).
// This function works by doing string modification on boot_device.
// Returns empty string on failure.
const std::string BootKernelDevice(const std::string& boot_device);


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
  DISALLOW_COPY_AND_ASSIGN(ScopedFilesystemUnmounter);
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
  DISALLOW_COPY_AND_ASSIGN(ScopedFdCloser);
};

// Utility class to delete a file when it goes out of scope.
class ScopedPathUnlinker {
 public:
  explicit ScopedPathUnlinker(const std::string& path) : path_(path) {}
  ~ScopedPathUnlinker() {
    if (unlink(path_.c_str()) < 0) {
      std::string err_message = strerror(errno);
      LOG(ERROR) << "Unable to unlink path " << path_ << ": " << err_message;
    }
  }
 private:
  const std::string path_;
  DISALLOW_COPY_AND_ASSIGN(ScopedPathUnlinker);
};

// Utility class to delete an empty directory when it goes out of scope.
class ScopedDirRemover {
 public:
  explicit ScopedDirRemover(const std::string& path) : path_(path) {}
  ~ScopedDirRemover() {
    if (rmdir(path_.c_str()) < 0)
      PLOG(ERROR) << "Unable to remove dir " << path_;
  }
 private:
  const std::string path_;
  DISALLOW_COPY_AND_ASSIGN(ScopedDirRemover);
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
