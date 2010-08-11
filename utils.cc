// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/utils.h"

#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "chromeos/obsolete_logging.h"
#include "update_engine/file_writer.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/subprocess.h"

using std::min;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace utils {

bool IsOfficialBuild() {
  OmahaRequestDeviceParams params;
  if (!params.Init("", "")) {
    return true;
  }
  return params.app_track != "buildbot-build" &&
      params.app_track != "developer-build";
}

bool WriteFile(const char* path, const char* data, int data_len) {
  DirectFileWriter writer;
  TEST_AND_RETURN_FALSE_ERRNO(0 == writer.Open(path,
                                               O_WRONLY | O_CREAT | O_TRUNC,
                                               0666));
  ScopedFileWriterCloser closer(&writer);
  TEST_AND_RETURN_FALSE_ERRNO(data_len == writer.Write(data, data_len));
  return true;
}

bool WriteAll(int fd, const void* buf, size_t count) {
  const char* c_buf = static_cast<const char*>(buf);
  ssize_t bytes_written = 0;
  while (bytes_written < static_cast<ssize_t>(count)) {
    ssize_t rc = write(fd, c_buf + bytes_written, count - bytes_written);
    TEST_AND_RETURN_FALSE_ERRNO(rc >= 0);
    bytes_written += rc;
  }
  return true;
}

bool PWriteAll(int fd, const void* buf, size_t count, off_t offset) {
  const char* c_buf = static_cast<const char*>(buf);
  ssize_t bytes_written = 0;
  while (bytes_written < static_cast<ssize_t>(count)) {
    ssize_t rc = pwrite(fd, c_buf + bytes_written, count - bytes_written,
                        offset + bytes_written);
    TEST_AND_RETURN_FALSE_ERRNO(rc >= 0);
    bytes_written += rc;
  }
  return true;
}

bool PReadAll(int fd, void* buf, size_t count, off_t offset,
              ssize_t* out_bytes_read) {
  char* c_buf = static_cast<char*>(buf);
  ssize_t bytes_read = 0;
  while (bytes_read < static_cast<ssize_t>(count)) {
    ssize_t rc = pread(fd, c_buf + bytes_read, count - bytes_read,
                       offset + bytes_read);
    TEST_AND_RETURN_FALSE_ERRNO(rc >= 0);
    if (rc == 0) {
      break;
    }
    bytes_read += rc;
  }
  *out_bytes_read = bytes_read;
  return true;

}

bool ReadFile(const std::string& path, std::vector<char>* out) {
  CHECK(out);
  FILE* fp = fopen(path.c_str(), "r");
  if (!fp)
    return false;
  const size_t kChunkSize = 1024;
  size_t read_size;
  do {
    char buf[kChunkSize];
    read_size = fread(buf, 1, kChunkSize, fp);
    if (read_size == 0)
      break;
    out->insert(out->end(), buf, buf + read_size);
  } while (read_size == kChunkSize);
  bool success = !ferror(fp);
  TEST_AND_RETURN_FALSE_ERRNO(fclose(fp) == 0);
  return success;
}

bool ReadFileToString(const std::string& path, std::string* out) {
  vector<char> data;
  bool success = ReadFile(path, &data);
  if (!success) {
    return false;
  }
  (*out) = string(&data[0], data.size());
  return true;
}

off_t FileSize(const string& path) {
  struct stat stbuf;
  int rc = stat(path.c_str(), &stbuf);
  CHECK_EQ(rc, 0);
  if (rc < 0)
    return rc;
  return stbuf.st_size;
}

void HexDumpArray(const unsigned char* const arr, const size_t length) {
  const unsigned char* const char_arr =
      reinterpret_cast<const unsigned char* const>(arr);
  LOG(INFO) << "Logging array of length: " << length;
  const unsigned int bytes_per_line = 16;
  for (uint32_t i = 0; i < length; i += bytes_per_line) {
    const unsigned int bytes_remaining = length - i;
    const unsigned int bytes_per_this_line = min(bytes_per_line,
                                                 bytes_remaining);
    char header[100];
    int r = snprintf(header, sizeof(header), "0x%08x : ", i);
    TEST_AND_RETURN(r == 13);
    string line = header;
    for (unsigned int j = 0; j < bytes_per_this_line; j++) {
      char buf[20];
      unsigned char c = char_arr[i + j];
      r = snprintf(buf, sizeof(buf), "%02x ", static_cast<unsigned int>(c));
      TEST_AND_RETURN(r == 3);
      line += buf;
    }
    LOG(INFO) << line;
  }
}

namespace {
class ScopedDirCloser {
 public:
  explicit ScopedDirCloser(DIR** dir) : dir_(dir) {}
  ~ScopedDirCloser() {
    if (dir_ && *dir_) {
      int r = closedir(*dir_);
      TEST_AND_RETURN_ERRNO(r == 0);
      *dir_ = NULL;
      dir_ = NULL;
    }
  }
 private:
  DIR** dir_;
};
}  // namespace {}

bool RecursiveUnlinkDir(const std::string& path) {
  struct stat stbuf;
  int r = lstat(path.c_str(), &stbuf);
  TEST_AND_RETURN_FALSE_ERRNO((r == 0) || (errno == ENOENT));
  if ((r < 0) && (errno == ENOENT))
    // path request is missing. that's fine.
    return true;
  if (!S_ISDIR(stbuf.st_mode)) {
    TEST_AND_RETURN_FALSE_ERRNO((unlink(path.c_str()) == 0) ||
                                (errno == ENOENT));
    // success or path disappeared before we could unlink.
    return true;
  }
  {
    // We have a dir, unlink all children, then delete dir
    DIR *dir = opendir(path.c_str());
    TEST_AND_RETURN_FALSE_ERRNO(dir);
    ScopedDirCloser dir_closer(&dir);
    struct dirent dir_entry;
    struct dirent *dir_entry_p;
    int err = 0;
    while ((err = readdir_r(dir, &dir_entry, &dir_entry_p)) == 0) {
      if (dir_entry_p == NULL) {
        // end of stream reached
        break;
      }
      // Skip . and ..
      if (!strcmp(dir_entry_p->d_name, ".") ||
          !strcmp(dir_entry_p->d_name, ".."))
        continue;
      TEST_AND_RETURN_FALSE(RecursiveUnlinkDir(path + "/" +
                                               dir_entry_p->d_name));
    }
    TEST_AND_RETURN_FALSE(err == 0);
  }
  // unlink dir
  TEST_AND_RETURN_FALSE_ERRNO((rmdir(path.c_str()) == 0) || (errno == ENOENT));
  return true;
}

string RootDevice(const string& partition_device) {
  FilePath device_path(partition_device);
  if (device_path.DirName().value() != "/dev") {
    return "";
  }
  string::const_iterator it = --partition_device.end();
  for (; it >= partition_device.begin(); --it) {
    if (!isdigit(*it))
      break;
  }
  // Some devices contain a p before the partitions. For example:
  // /dev/mmc0p4 should be shortened to /dev/mmc0.
  if (*it == 'p')
    --it;
  return string(partition_device.begin(), it + 1);
}

string PartitionNumber(const string& partition_device) {
  CHECK(!partition_device.empty());
  string::const_iterator it = --partition_device.end();
  for (; it >= partition_device.begin(); --it) {
    if (!isdigit(*it))
      break;
  }
  return string(it + 1, partition_device.end());
}

string SysfsBlockDevice(const string& device) {
  FilePath device_path(device);
  if (device_path.DirName().value() != "/dev") {
    return "";
  }
  return FilePath("/sys/block").Append(device_path.BaseName()).value();
}

bool IsRemovableDevice(const std::string& device) {
  string sysfs_block = SysfsBlockDevice(device);
  string removable;
  if (sysfs_block.empty() ||
      !file_util::ReadFileToString(FilePath(sysfs_block).Append("removable"),
                                   &removable)) {
    return false;
  }
  TrimWhitespaceASCII(removable, TRIM_ALL, &removable);
  return removable == "1";
}

std::string ErrnoNumberAsString(int err) {
  char buf[100];
  buf[0] = '\0';
  return strerror_r(err, buf, sizeof(buf));
}

std::string NormalizePath(const std::string& path, bool strip_trailing_slash) {
  string ret;
  bool last_insert_was_slash = false;
  for (string::const_iterator it = path.begin(); it != path.end(); ++it) {
    if (*it == '/') {
      if (last_insert_was_slash)
        continue;
      last_insert_was_slash = true;
    } else {
      last_insert_was_slash = false;
    }
    ret.push_back(*it);
  }
  if (strip_trailing_slash && last_insert_was_slash) {
    string::size_type last_non_slash = ret.find_last_not_of('/');
    if (last_non_slash != string::npos) {
      ret.resize(last_non_slash + 1);
    } else {
      ret = "";
    }
  }
  return ret;
}

bool FileExists(const char* path) {
  struct stat stbuf;
  return 0 == lstat(path, &stbuf);
}

std::string TempFilename(string path) {
  static const string suffix("XXXXXX");
  CHECK(StringHasSuffix(path, suffix));
  do {
    string new_suffix;
    for (unsigned int i = 0; i < suffix.size(); i++) {
      int r = rand() % (26 * 2 + 10);  // [a-zA-Z0-9]
      if (r < 26)
        new_suffix.append(1, 'a' + r);
      else if (r < (26 * 2))
        new_suffix.append(1, 'A' + r - 26);
      else
        new_suffix.append(1, '0' + r - (26 * 2));
    }
    CHECK_EQ(new_suffix.size(), suffix.size());
    path.resize(path.size() - new_suffix.size());
    path.append(new_suffix);
  } while (FileExists(path.c_str()));
  return path;
}

bool MakeTempFile(const std::string& filename_template,
                  std::string* filename,
                  int* fd) {
  DCHECK(filename || fd);
  vector<char> buf(filename_template.size() + 1);
  memcpy(&buf[0], filename_template.data(), filename_template.size());
  buf[filename_template.size()] = '\0';

  int mkstemp_fd = mkstemp(&buf[0]);
  TEST_AND_RETURN_FALSE_ERRNO(mkstemp_fd >= 0);
  if (filename) {
    *filename = &buf[0];
  }
  if (fd) {
    *fd = mkstemp_fd;
  } else {
    close(mkstemp_fd);
  }
  return true;
}

bool MakeTempDirectory(const std::string& dirname_template,
                       std::string* dirname) {
  DCHECK(dirname);
  vector<char> buf(dirname_template.size() + 1);
  memcpy(&buf[0], dirname_template.data(), dirname_template.size());
  buf[dirname_template.size()] = '\0';

  char* return_code = mkdtemp(&buf[0]);
  TEST_AND_RETURN_FALSE_ERRNO(return_code != NULL);
  *dirname = &buf[0];
  return true;
}

bool StringHasSuffix(const std::string& str, const std::string& suffix) {
  if (suffix.size() > str.size())
    return false;
  return 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

bool StringHasPrefix(const std::string& str, const std::string& prefix) {
  if (prefix.size() > str.size())
    return false;
  return 0 == str.compare(0, prefix.size(), prefix);
}

const string BootDevice() {
  string proc_cmdline;
  if (!ReadFileToString("/proc/cmdline", &proc_cmdline))
    return "";
  // look for "root=" in the command line
  string::size_type pos = 0;
  if (!StringHasPrefix(proc_cmdline, "root=")) {
    pos = proc_cmdline.find(" root=") + 1;
  }
  if (pos == string::npos) {
    // can't find root=
    return "";
  }
  // at this point, pos is the point in the string where "root=" starts
  string ret;
  pos += strlen("root=");  // advance to the device name itself
  while (pos < proc_cmdline.size()) {
    char c = proc_cmdline[pos];
    if (c == ' ')
      break;
    ret += c;
    pos++;
  }
  return ret;
  // TODO(adlr): use findfs to figure out UUID= or LABEL= filesystems
}

const string BootKernelDevice(const std::string& boot_device) {
  // Currntly this assumes the last digit of the boot device is
  // 3, 5, or 7, and changes it to 2, 4, or 6, respectively, to
  // get the kernel device.
  string ret = boot_device;
  if (ret.empty())
    return ret;
  char last_char = ret[ret.size() - 1];
  if (last_char == '3' || last_char == '5' || last_char == '7') {
    ret[ret.size() - 1] = last_char - 1;
    return ret;
  }
  return "";
}

bool MountFilesystem(const string& device,
                     const string& mountpoint,
                     unsigned long mountflags) {
  int rc = mount(device.c_str(), mountpoint.c_str(), "ext3", mountflags, NULL);
  if (rc < 0) {
    string msg = ErrnoNumberAsString(errno);
    LOG(ERROR) << "Unable to mount destination device: " << msg << ". "
               << device << " on " << mountpoint;
    return false;
  }
  return true;
}

bool UnmountFilesystem(const string& mountpoint) {
  TEST_AND_RETURN_FALSE_ERRNO(umount(mountpoint.c_str()) == 0);
  return true;
}

bool GetBootloader(BootLoader* out_bootloader) {
  // For now, hardcode to syslinux.
  *out_bootloader = BootLoader_SYSLINUX;
  return true;
}

const char* GetGErrorMessage(const GError* error) {
  if (!error)
    return "Unknown error.";
  return error->message;
}

bool Reboot() {
  vector<string> command;
  command.push_back("/sbin/shutdown");
  command.push_back("-r");
  command.push_back("now");
  int rc = 0;
  Subprocess::SynchronousExec(command, &rc);
  TEST_AND_RETURN_FALSE(rc == 0);
  return true;
}

bool SetProcessPriority(ProcessPriority priority) {
  int prio = static_cast<int>(priority);
  LOG(INFO) << "Setting process priority to " << prio;
  TEST_AND_RETURN_FALSE(setpriority(PRIO_PROCESS, 0, prio) == 0);
  return true;
}

int ComparePriorities(ProcessPriority priority_lhs,
                      ProcessPriority priority_rhs) {
  return static_cast<int>(priority_rhs) - static_cast<int>(priority_lhs);
}

const char* const kStatefulPartition = "/mnt/stateful_partition";

}  // namespace utils

}  // namespace chromeos_update_engine
