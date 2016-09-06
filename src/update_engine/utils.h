// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UTILS_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UTILS_H__

#include <errno.h>

#include <algorithm>
#include <chrono>
#include <set>
#include <string>
#include <unistd.h>
#include <vector>

#include <glib.h>

#include "update_engine/action.h"
#include "update_engine/action_processor.h"

namespace chromeos_update_engine {

class SystemState;

namespace utils {

// Days doesn't have an standard type but it is a common unit in our code.
using days_t = std::chrono::duration<int, std::ratio<86400>>;

// Returns true if this is an official Chrome OS build, false otherwise.
bool IsOfficialBuild();

// Returns true if string starts with https://
bool IsHTTPS(const std::string& url);

// Returns the HWID or an empty string on error.
std::string GetHardwareClass();

// Returns the boot_id or an empty string on error.
std::string GetBootId();

// Returns the machineid or an empty string on error.
std::string GetMachineId();

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

// Opens |path| for reading and appends its entire content to the container
// pointed to by |out_p|. Returns true upon successfully reading all of the
// file's content, false otherwise, in which case the state of the output
// container is unknown.
bool ReadFile(const std::string& path, std::vector<char>* out_p);
bool ReadFile(const std::string& path, std::string* out_p);

// Invokes |cmd| in a pipe and appends its stdout to the container pointed to by
// |out_p|. Returns true upon successfully reading all of the output, false
// otherwise, in which case the state of the output container is unknown.
bool ReadPipe(const std::string& cmd, std::vector<char>* out_p);
bool ReadPipe(const std::string& cmd, std::string* out_p);

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

// Returns true if |path| exists and is a symbolic link.
bool IsSymlink(const char* path);

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

// Calls mkdtemp() with the template passed. Returns the generated dirname
// in the dirname param. Returns TRUE on success. dirname must not be NULL.
bool MakeTempDirectory(const std::string& dirname_template,
                       std::string* dirname);

// Deletes a directory and all its contents synchronously. Returns true
// on success. This may be called with a regular file--it will just unlink it.
// This WILL cross filesystem boundaries.
bool RecursiveUnlinkDir(const std::string& path);

// Returns the root device for a partition. For example,
// RootDevice("/dev/sda3") returns "/dev/sda". Returns an empty string
// if the input device is not of the "/dev/xyz" form.
std::string RootDevice(const std::string& partition_device);

// Returns the partition number, as a string, of partition_device. For example,
// PartitionNumber("/dev/sda3") returns "3".
std::string PartitionNumber(const std::string& partition_device);

// Returns the sysfs block device for a root block device. For
// example, SysfsBlockDevice("/dev/sda") returns
// "/sys/block/sda". Returns an empty string if the input device is
// not of the "/dev/xyz" form.
std::string SysfsBlockDevice(const std::string& device);

// Returns true if the root |device| (e.g., "/dev/sdb") is known to be
// removable, false otherwise.
bool IsRemovableDevice(const std::string& device);

// Synchronously mount or unmount a filesystem. Return true on success.
// Mounts as ext3 with default options.
bool MountFilesystem(const std::string& device, const std::string& mountpoint,
                     unsigned long flags);
bool UnmountFilesystem(const std::string& mountpoint);

// Get the size in bytes of a block device or regular file.
// Returns true on success, false otherwise.
bool GetDeviceSize(const std::string& device, off_t* size);
bool GetDeviceSizeFromFD(int fd, off_t* size);

// Returns the string representation of the given UTC time.
// such as "11/14/2011 14:05:30 UTC".
std::string ToString(const std::chrono::system_clock::time_point& tp);

// Returns true or false depending on the value of b.
std::string ToString(bool b);

enum BootLoader {
  BootLoader_SYSLINUX = 0,
  BootLoader_CHROME_FIRMWARE = 1
};
// Detects which bootloader this system uses and returns it via the out
// param. Returns true on success.
bool GetBootloader(BootLoader* out_bootloader);

// Returns the error message, if any, from a GError pointer. Frees the GError
// object and resets error to NULL.
std::string GetAndFreeGError(GError** error);

// Initiates a system reboot. Returns true on success, false otherwise.
bool Reboot();

// Schedules a Main Loop callback to trigger the crash reporter to perform an
// upload as if this process had crashed.
void ScheduleCrashReporterUpload();

// Fuzzes an integer |value| randomly in the range:
// [value - range / 2, value + range - range / 2]
int32_t FuzzInt(int32_t value, uint32_t range);

// Log a string in hex to LOG(INFO). Useful for debugging.
void HexDumpArray(const unsigned char* const arr, const size_t length);
inline void HexDumpString(const std::string& str) {
  HexDumpArray(reinterpret_cast<const unsigned char*>(str.data()), str.size());
}
inline void HexDumpVector(const std::vector<char>& vect) {
  HexDumpArray(reinterpret_cast<const unsigned char*>(&vect[0]), vect.size());
}

bool StringHasSuffix(const std::string& str, const std::string& suffix);
bool StringHasPrefix(const std::string& str, const std::string& prefix);

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

template<typename ValueType>
void ApplyMap(std::vector<ValueType>* collection,
              const std::map<ValueType, ValueType>& the_map) {
  for (typename std::vector<ValueType>::iterator it = collection->begin();
       it != collection->end(); ++it) {
    typename std::map<ValueType, ValueType>::const_iterator map_it =
      the_map.find(*it);
    if (map_it != the_map.end()) {
      *it = map_it->second;
    }
  }
}

// Returns the currently booted device. "/dev/sda3", for example.
// This will not interpret LABEL= or UUID=. You'll need to use findfs
// or something with equivalent funcionality to interpret those.
const std::string BootDevice();

// Assumes data points to a Closure. Runs it and returns FALSE;
gboolean GlibRunClosure(gpointer data);

// Converts seconds into human readable notation including days, hours, minutes
// and seconds. For example, 185 will yield 3m5s, 4300 will yield 1h11m40s, and
// 360000 will yield 4d4h0m0s.  Zero padding not applied. Seconds are always
// shown in the result.
std::string FormatSecs(unsigned secs);

// Converts a duration into human readable notation including days, hours,
// minutes, seconds and fractions of a second down to microsecond granularity,
// as necessary; for example, an output of 5d2h0m15.053s means that the input
// time was precise to the milliseconds only. Zero padding not applied, except
// for fractions. Seconds are always shown, but fractions thereof are only shown
// when applicable.
std::string ToString(std::chrono::microseconds delta);

// This method transforms the given error code to be suitable for
// error classification purposes by removing the higher order bits and
// aggregating error codes beyond the enum range, etc. This method is
// idempotent, i.e. if called with a value previously returned by this method,
// it'll return the same value again.
ActionExitCode GetBaseErrorCode(ActionExitCode code);

// Returns a string representation of the ActionExitCodes (either the base
// error codes or the bit flags) for logging purposes.
std::string CodeToString(ActionExitCode code);

}  // namespace utils


// Class to unmount FS when object goes out of scope
class ScopedFilesystemUnmounter {
 public:
  explicit ScopedFilesystemUnmounter(const std::string& mountpoint)
      : mountpoint_(mountpoint),
        should_unmount_(true) {}
  ~ScopedFilesystemUnmounter() {
    if (should_unmount_) {
      utils::UnmountFilesystem(mountpoint_);
    }
  }
  void set_should_unmount(bool unmount) { should_unmount_ = unmount; }
 private:
  const std::string mountpoint_;
  bool should_unmount_;
  DISALLOW_COPY_AND_ASSIGN(ScopedFilesystemUnmounter);
};

// Utility class to delete a file when it goes out of scope.
class ScopedPathUnlinker {
 public:
  explicit ScopedPathUnlinker(const std::string& path)
      : path_(path),
        should_remove_(true) {}
  ~ScopedPathUnlinker() {
    if (should_remove_ && unlink(path_.c_str()) < 0) {
      std::string err_message = strerror(errno);
      LOG(ERROR) << "Unable to unlink path " << path_ << ": " << err_message;
    }
  }
  void set_should_remove(bool should_remove) { should_remove_ = should_remove; }

 private:
  const std::string path_;
  bool should_remove_;
  DISALLOW_COPY_AND_ASSIGN(ScopedPathUnlinker);
};

// Utility class to delete an empty directory when it goes out of scope.
class ScopedDirRemover {
 public:
  explicit ScopedDirRemover(const std::string& path)
      : path_(path),
        should_remove_(true) {}
  ~ScopedDirRemover() {
    if (should_remove_ && (rmdir(path_.c_str()) < 0)) {
      PLOG(ERROR) << "Unable to remove dir " << path_;
    }
  }
  void set_should_remove(bool should_remove) { should_remove_ = should_remove; }

 protected:
  const std::string path_;

 private:
  bool should_remove_;
  DISALLOW_COPY_AND_ASSIGN(ScopedDirRemover);
};

// Utility class to unmount a filesystem mounted on a temporary directory and
// delete the temporary directory when it goes out of scope.
class ScopedTempUnmounter : public ScopedDirRemover {
 public:
  explicit ScopedTempUnmounter(const std::string& path) :
      ScopedDirRemover(path) {}
  ~ScopedTempUnmounter() {
    utils::UnmountFilesystem(path_);
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedTempUnmounter);
};

// A little object to call ActionComplete on the ActionProcessor when
// it's destructed.
class ScopedActionCompleter {
 public:
  explicit ScopedActionCompleter(ActionProcessor* processor,
                                 AbstractAction* action)
      : processor_(processor),
        action_(action),
        code_(kActionCodeError),
        should_complete_(true) {}
  ~ScopedActionCompleter() {
    if (should_complete_)
      processor_->ActionComplete(action_, code_);
  }
  void set_code(ActionExitCode code) { code_ = code; }
  void set_should_complete(bool should_complete) {
    should_complete_ = should_complete;
  }

 private:
  ActionProcessor* processor_;
  AbstractAction* action_;
  ActionExitCode code_;
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

#define TEST_AND_RETURN_FALSE_ERRCODE(_x)                                      \
  do {                                                                         \
    errcode_t _error = (_x);                                                   \
    if (_error) {                                                              \
      errno = _error;                                                          \
      LOG(ERROR) << #_x " failed: " << _error;                                 \
      return false;                                                            \
    }                                                                          \
  } while (0)



#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UTILS_H__
