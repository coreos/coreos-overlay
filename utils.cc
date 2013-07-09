// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/utils.h"

#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/rand_util.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <glib.h>
#include <google/protobuf/stubs/common.h>
#include <rootdev/rootdev.h>

#include "update_engine/file_writer.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/subprocess.h"
#include "update_engine/system_state.h"
#include "update_engine/update_attempter.h"

using base::Time;
using base::TimeDelta;
using std::min;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {

// The following constants control how UnmountFilesystem should retry if
// umount() fails with an errno EBUSY, i.e. retry 5 times over the course of
// one second.
const int kUnmountMaxNumOfRetries = 5;
const int kUnmountRetryIntervalInMicroseconds = 200 * 1000;  // 200 ms

}  // namespace

namespace utils {

static const char kBootId[] = "/proc/sys/kernel/random/boot_id";
static const char kDevImageMarker[] = "/root/.dev_mode";
const char* const kStatefulPartition = "/mnt/stateful_partition";

// Cgroup container is created in update-engine's upstart script located at
// /etc/init/update-engine.conf.
static const char kCGroupDir[] = "/sys/fs/cgroup/cpu/update-engine";

bool IsOfficialBuild() {
  return !file_util::PathExists(FilePath(kDevImageMarker));
}

bool IsNormalBootMode() {
  // TODO(petkov): Convert to a library call once a crossystem library is
  // available (crosbug.com/13291).
  int exit_code = 0;
  vector<string> cmd(1, "/usr/bin/crossystem");
  cmd.push_back("devsw_boot?1");

  // Assume dev mode if the dev switch is set to 1 and there was no error
  // executing crossystem. Assume normal mode otherwise.
  bool success = Subprocess::SynchronousExec(cmd, &exit_code, NULL);
  bool dev_mode = success && exit_code == 0;
  LOG_IF(INFO, dev_mode) << "Booted in dev mode.";
  return !dev_mode;
}

string GetHardwareClass() {
  // TODO(petkov): Convert to a library call once a crossystem library is
  // available (crosbug.com/13291).
  int exit_code = 0;
  vector<string> cmd(1, "/usr/bin/crossystem");
  cmd.push_back("hwid");

  string hwid;
  bool success = Subprocess::SynchronousExec(cmd, &exit_code, &hwid);
  if (success && !exit_code) {
    TrimWhitespaceASCII(hwid, TRIM_ALL, &hwid);
    return hwid;
  }
  LOG(ERROR) << "Unable to read HWID (" << exit_code << ") " << hwid;
  return "";
}

string GetBootId() {
  string id;
  string guid;
  if (!file_util::ReadFileToString(FilePath(kBootId), &id)) {
    LOG(ERROR) << "Unable to read boot_id";
    return "";
  }
  TrimWhitespaceASCII(id, TRIM_ALL, &id);

  // Make it look like the other UUIDs in the payload
  guid.append(1, '{');
  guid.append(id);
  guid.append(1, '}');
  return guid;
}

bool WriteFile(const char* path, const char* data, int data_len) {
  DirectFileWriter writer;
  TEST_AND_RETURN_FALSE_ERRNO(0 == writer.Open(path,
                                               O_WRONLY | O_CREAT | O_TRUNC,
                                               0600));
  ScopedFileWriterCloser closer(&writer);
  TEST_AND_RETURN_FALSE_ERRNO(writer.Write(data, data_len));
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
  size_t bytes_written = 0;
  int num_attempts = 0;
  while (bytes_written < count) {
    num_attempts++;
    ssize_t rc = pwrite(fd, c_buf + bytes_written, count - bytes_written,
                        offset + bytes_written);
    // TODO(garnold) for debugging failure in chromium-os:31077; to be removed.
    if (rc < 0) {
      PLOG(ERROR) << "pwrite error; num_attempts=" << num_attempts
                  << " bytes_written=" << bytes_written
                  << " count=" << count << " offset=" << offset;
    }
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

// Append |nbytes| of content from |buf| to the vector pointed to by either
// |vec_p| or |str_p|.
static void AppendBytes(const char* buf, size_t nbytes,
                        std::vector<char>* vec_p) {
  CHECK(buf);
  CHECK(vec_p);
  vec_p->insert(vec_p->end(), buf, buf + nbytes);
}
static void AppendBytes(const char* buf, size_t nbytes,
                        std::string* str_p) {
  CHECK(buf);
  CHECK(str_p);
  str_p->append(buf, nbytes);
}

// Reads from an open file |fp|, appending the read content to the container
// pointer to by |out_p|.  Returns true upon successful reading all of the
// file's content, false otherwise.
template <class T>
static bool Read(FILE* fp, T* out_p) {
  CHECK(fp);
  char buf[1024];
  while (size_t nbytes = fread(buf, 1, sizeof(buf), fp))
    AppendBytes(buf, nbytes, out_p);
  return feof(fp) && !ferror(fp);
}

// Opens a file |path| for reading, then uses |append_func| to append its
// content to a container |out_p|.
template <class T>
static bool ReadFileAndAppend(const std::string& path, T* out_p) {
  FILE* fp = fopen(path.c_str(), "r");
  if (!fp)
    return false;
  bool success = Read(fp, out_p);
  return (success && !fclose(fp));
}

// Invokes a pipe |cmd|, then uses |append_func| to append its stdout to a
// container |out_p|.
template <class T>
static bool ReadPipeAndAppend(const std::string& cmd, T* out_p) {
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp)
    return false;
  bool success = Read(fp, out_p);
  return (success && pclose(fp) >= 0);
}


bool ReadFile(const std::string& path, std::vector<char>* out_p) {
  return ReadFileAndAppend(path, out_p);
}

bool ReadFile(const std::string& path, std::string* out_p) {
  return ReadFileAndAppend(path, out_p);
}

bool ReadPipe(const std::string& cmd, std::vector<char>* out_p) {
  return ReadPipeAndAppend(cmd, out_p);
}

bool ReadPipe(const std::string& cmd, std::string* out_p) {
  return ReadPipeAndAppend(cmd, out_p);
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

bool IsSymlink(const char* path) {
  struct stat stbuf;
  return lstat(path, &stbuf) == 0 && S_ISLNK(stbuf.st_mode) != 0;
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

const std::string BootDevice() {
  char boot_path[PATH_MAX];
  // Resolve the boot device path fully, including dereferencing
  // through dm-verity.
  int ret = rootdev(boot_path, sizeof(boot_path), true, false);

  if (ret < 0) {
    LOG(ERROR) << "rootdev failed to find the root device";
    return "";
  }
  LOG_IF(WARNING, ret > 0) << "rootdev found a device name with no device node";

  // This local variable is used to construct the return string and is not
  // passed around after use.
  return boot_path;
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
  for (int num_retries = 0; ; ++num_retries) {
    if (umount(mountpoint.c_str()) == 0)
      break;

    TEST_AND_RETURN_FALSE_ERRNO(errno == EBUSY &&
                                num_retries < kUnmountMaxNumOfRetries);
    g_usleep(kUnmountRetryIntervalInMicroseconds);
  }
  return true;
}

bool GetFilesystemSize(const std::string& device,
                       int* out_block_count,
                       int* out_block_size) {
  int fd = HANDLE_EINTR(open(device.c_str(), O_RDONLY));
  TEST_AND_RETURN_FALSE(fd >= 0);
  ScopedFdCloser fd_closer(&fd);
  return GetFilesystemSizeFromFD(fd, out_block_count, out_block_size);
}

bool GetFilesystemSizeFromFD(int fd,
                             int* out_block_count,
                             int* out_block_size) {
  TEST_AND_RETURN_FALSE(fd >= 0);

  // Determine the ext3 filesystem size by directly reading the block count and
  // block size information from the superblock. See include/linux/ext3_fs.h for
  // more details on the structure.
  ssize_t kBufferSize = 16 * sizeof(uint32_t);
  char buffer[kBufferSize];
  const int kSuperblockOffset = 1024;
  if (HANDLE_EINTR(pread(fd, buffer, kBufferSize, kSuperblockOffset)) !=
      kBufferSize) {
    PLOG(ERROR) << "Unable to determine file system size:";
    return false;
  }
  uint32_t block_count;  // ext3_fs.h: ext3_super_block.s_blocks_count
  uint32_t log_block_size;  // ext3_fs.h: ext3_super_block.s_log_block_size
  uint16_t magic;  // ext3_fs.h: ext3_super_block.s_magic
  memcpy(&block_count, &buffer[1 * sizeof(int32_t)], sizeof(block_count));
  memcpy(&log_block_size, &buffer[6 * sizeof(int32_t)], sizeof(log_block_size));
  memcpy(&magic, &buffer[14 * sizeof(int32_t)], sizeof(magic));
  block_count = le32toh(block_count);
  const int kExt3MinBlockLogSize = 10;  // ext3_fs.h: EXT3_MIN_BLOCK_LOG_SIZE
  log_block_size = le32toh(log_block_size) + kExt3MinBlockLogSize;
  magic = le16toh(magic);

  // Sanity check the parameters.
  const uint16_t kExt3SuperMagic = 0xef53;  // ext3_fs.h: EXT3_SUPER_MAGIC
  TEST_AND_RETURN_FALSE(magic == kExt3SuperMagic);
  const int kExt3MinBlockSize = 1024;  // ext3_fs.h: EXT3_MIN_BLOCK_SIZE
  const int kExt3MaxBlockSize = 4096;  // ext3_fs.h: EXT3_MAX_BLOCK_SIZE
  int block_size = 1 << log_block_size;
  TEST_AND_RETURN_FALSE(block_size >= kExt3MinBlockSize &&
                        block_size <= kExt3MaxBlockSize);
  TEST_AND_RETURN_FALSE(block_count > 0);

  if (out_block_count) {
    *out_block_count = block_count;
  }
  if (out_block_size) {
    *out_block_size = block_size;
  }
  return true;
}

bool GetBootloader(BootLoader* out_bootloader) {
  // For now, hardcode to syslinux.
  *out_bootloader = BootLoader_SYSLINUX;
  return true;
}

string GetAndFreeGError(GError** error) {
  if (!*error) {
    return "Unknown GLib error.";
  }
  string message =
      base::StringPrintf("GError(%d): %s",
                         (*error)->code,
                         (*error)->message ? (*error)->message : "(unknown)");
  g_error_free(*error);
  *error = NULL;
  return message;
}

bool Reboot() {
  vector<string> command;
  command.push_back("/sbin/shutdown");
  command.push_back("-r");
  command.push_back("now");
  int rc = 0;
  Subprocess::SynchronousExec(command, &rc, NULL);
  TEST_AND_RETURN_FALSE(rc == 0);
  return true;
}

namespace {
// Do the actual trigger. We do it as a main-loop callback to (try to) get a
// consistent stack trace.
gboolean TriggerCrashReporterUpload(void* unused) {
  pid_t pid = fork();
  CHECK(pid >= 0) << "fork failed";  // fork() failed. Something is very wrong.
  if (pid == 0) {
    // We are the child. Crash.
    abort();  // never returns
  }
  // We are the parent. Wait for child to terminate.
  pid_t result = waitpid(pid, NULL, 0);
  LOG_IF(ERROR, result < 0) << "waitpid() failed";
  return FALSE;  // Don't call this callback again
}
}  // namespace {}

void ScheduleCrashReporterUpload() {
  g_idle_add(&TriggerCrashReporterUpload, NULL);
}

bool SetCpuShares(CpuShares shares) {
  string string_shares = base::IntToString(static_cast<int>(shares));
  string cpu_shares_file = string(utils::kCGroupDir) + "/cpu.shares";
  LOG(INFO) << "Setting cgroup cpu shares to  " << string_shares;
  if(utils::WriteFile(cpu_shares_file.c_str(), string_shares.c_str(),
                      string_shares.size())){
    return true;
  } else {
    LOG(ERROR) << "Failed to change cgroup cpu shares to "<< string_shares
               << " using " << cpu_shares_file;
    return false;
  }
}

int CompareCpuShares(CpuShares shares_lhs,
                     CpuShares shares_rhs) {
  return static_cast<int>(shares_lhs) - static_cast<int>(shares_rhs);
}

int FuzzInt(int value, unsigned int range) {
  int min = value - range / 2;
  int max = value + range - range / 2;
  return base::RandInt(min, max);
}

gboolean GlibRunClosure(gpointer data) {
  google::protobuf::Closure* callback =
      reinterpret_cast<google::protobuf::Closure*>(data);
  callback->Run();
  return FALSE;
}

string FormatSecs(unsigned secs) {
  return FormatTimeDelta(TimeDelta::FromSeconds(secs));
}

string FormatTimeDelta(TimeDelta delta) {
  // Canonicalize into days, hours, minutes, seconds and microseconds.
  unsigned days = delta.InDays();
  delta -= TimeDelta::FromDays(days);
  unsigned hours = delta.InHours();
  delta -= TimeDelta::FromHours(hours);
  unsigned mins = delta.InMinutes();
  delta -= TimeDelta::FromMinutes(mins);
  unsigned secs = delta.InSeconds();
  delta -= TimeDelta::FromSeconds(secs);
  unsigned usecs = delta.InMicroseconds();

  // Construct and return string.
  string str;
  if (days)
    base::StringAppendF(&str, "%ud", days);
  if (days || hours)
    base::StringAppendF(&str, "%uh", hours);
  if (days || hours || mins)
    base::StringAppendF(&str, "%um", mins);
  base::StringAppendF(&str, "%u", secs);
  if (usecs) {
    int width = 6;
    while ((usecs / 10) * 10 == usecs) {
      usecs /= 10;
      width--;
    }
    base::StringAppendF(&str, ".%0*u", width, usecs);
  }
  base::StringAppendF(&str, "s");
  return str;
}

string ToString(const Time utc_time) {
  Time::Exploded exp_time;
  utc_time.UTCExplode(&exp_time);
  return StringPrintf("%d/%d/%d %d:%02d:%02d GMT",
                      exp_time.month,
                      exp_time.day_of_month,
                      exp_time.year,
                      exp_time.hour,
                      exp_time.minute,
                      exp_time.second);
}

string ToString(bool b) {
  return (b ? "true" : "false");
}

ActionExitCode GetBaseErrorCode(ActionExitCode code) {
  // Ignore the higher order bits in the code by applying the mask as
  // we want the enumerations to be in the small contiguous range
  // with values less than kActionCodeUmaReportedMax.
  ActionExitCode base_code = static_cast<ActionExitCode>(code & ~kSpecialFlags);

  // Make additional adjustments required for UMA and error classification.
  // TODO(jaysri): Move this logic to UeErrorCode.cc when we fix
  // chromium-os:34369.
  if (base_code >= kActionCodeOmahaRequestHTTPResponseBase) {
    // Since we want to keep the enums to a small value, aggregate all HTTP
    // errors into this one bucket for UMA and error classification purposes.
    LOG(INFO) << "Converting error code " << base_code
              << " to kActionCodeOmahaErrorInHTTPResponse";
    base_code = kActionCodeOmahaErrorInHTTPResponse;
  }

  return base_code;
}

// Returns a printable version of the various flags denoted in the higher order
// bits of the given code. Returns an empty string if none of those bits are
// set.
string GetFlagNames(uint32_t code) {
  uint32_t flags = code & kSpecialFlags;
  string flag_names;
  string separator = "";
  for(size_t i = 0; i < sizeof(flags) * 8; i++) {
    uint32_t flag = flags & (1 << i);
    if (flag) {
      flag_names += separator + CodeToString(static_cast<ActionExitCode>(flag));
      separator = ", ";
    }
  }

  return flag_names;
}

void SendErrorCodeToUma(SystemState* system_state, ActionExitCode code) {
  if (!system_state)
    return;

  ActionExitCode uma_error_code = GetBaseErrorCode(code);

  // If the code doesn't have flags computed already, compute them now based on
  // the state of the current update attempt.
  uint32_t flags = code & kSpecialFlags;
  if (!flags)
    flags = system_state->update_attempter()->GetErrorCodeFlags();

  // Determine the UMA bucket depending on the flags. But, ignore the resumed
  // flag, as it's perfectly normal for production devices to resume their
  // downloads and so we want to record those cases also in NormalErrorCodes
  // bucket.
  string metric = (flags & ~kActionCodeResumedFlag) ?
      "Installer.DevModeErrorCodes" : "Installer.NormalErrorCodes";

  LOG(INFO) << "Sending error code " << uma_error_code
            << " (" << CodeToString(uma_error_code) << ")"
            << " to UMA metric: " << metric
            << ". Flags = " << (flags ? GetFlagNames(flags) : "None");

  system_state->metrics_lib()->SendEnumToUMA(metric,
                                             uma_error_code,
                                             kActionCodeUmaReportedMax);
}

string CodeToString(ActionExitCode code) {
  // If the given code has both parts (i.e. the error code part and the flags
  // part) then strip off the flags part since the switch statement below
  // has case statements only for the base error code or a single flag but
  // doesn't support any combinations of those.
  if ((code & kSpecialFlags) && (code & ~kSpecialFlags))
    code = static_cast<ActionExitCode>(code & ~kSpecialFlags);
  switch (code) {
    case kActionCodeSuccess: return "kActionCodeSuccess";
    case kActionCodeError: return "kActionCodeError";
    case kActionCodeOmahaRequestError: return "kActionCodeOmahaRequestError";
    case kActionCodeOmahaResponseHandlerError:
      return "kActionCodeOmahaResponseHandlerError";
    case kActionCodeFilesystemCopierError:
      return "kActionCodeFilesystemCopierError";
    case kActionCodePostinstallRunnerError:
      return "kActionCodePostinstallRunnerError";
    case kActionCodeSetBootableFlagError:
      return "kActionCodeSetBootableFlagError";
    case kActionCodeInstallDeviceOpenError:
      return "kActionCodeInstallDeviceOpenError";
    case kActionCodeKernelDeviceOpenError:
      return "kActionCodeKernelDeviceOpenError";
    case kActionCodeDownloadTransferError:
      return "kActionCodeDownloadTransferError";
    case kActionCodePayloadHashMismatchError:
      return "kActionCodePayloadHashMismatchError";
    case kActionCodePayloadSizeMismatchError:
      return "kActionCodePayloadSizeMismatchError";
    case kActionCodeDownloadPayloadVerificationError:
      return "kActionCodeDownloadPayloadVerificationError";
    case kActionCodeDownloadNewPartitionInfoError:
      return "kActionCodeDownloadNewPartitionInfoError";
    case kActionCodeDownloadWriteError:
      return "kActionCodeDownloadWriteError";
    case kActionCodeNewRootfsVerificationError:
      return "kActionCodeNewRootfsVerificationError";
    case kActionCodeNewKernelVerificationError:
      return "kActionCodeNewKernelVerificationError";
    case kActionCodeSignedDeltaPayloadExpectedError:
      return "kActionCodeSignedDeltaPayloadExpectedError";
    case kActionCodeDownloadPayloadPubKeyVerificationError:
      return "kActionCodeDownloadPayloadPubKeyVerificationError";
    case kActionCodePostinstallBootedFromFirmwareB:
      return "kActionCodePostinstallBootedFromFirmwareB";
    case kActionCodeDownloadStateInitializationError:
      return "kActionCodeDownloadStateInitializationError";
    case kActionCodeDownloadInvalidMetadataMagicString:
      return "kActionCodeDownloadInvalidMetadataMagicString";
    case kActionCodeDownloadSignatureMissingInManifest:
      return "kActionCodeDownloadSignatureMissingInManifest";
    case kActionCodeDownloadManifestParseError:
      return "kActionCodeDownloadManifestParseError";
    case kActionCodeDownloadMetadataSignatureError:
      return "kActionCodeDownloadMetadataSignatureError";
    case kActionCodeDownloadMetadataSignatureVerificationError:
      return "kActionCodeDownloadMetadataSignatureVerificationError";
    case kActionCodeDownloadMetadataSignatureMismatch:
      return "kActionCodeDownloadMetadataSignatureMismatch";
    case kActionCodeDownloadOperationHashVerificationError:
      return "kActionCodeDownloadOperationHashVerificationError";
    case kActionCodeDownloadOperationExecutionError:
      return "kActionCodeDownloadOperationExecutionError";
    case kActionCodeDownloadOperationHashMismatch:
      return "kActionCodeDownloadOperationHashMismatch";
    case kActionCodeOmahaRequestEmptyResponseError:
      return "kActionCodeOmahaRequestEmptyResponseError";
    case kActionCodeOmahaRequestXMLParseError:
      return "kActionCodeOmahaRequestXMLParseError";
    case kActionCodeDownloadInvalidMetadataSize:
      return "kActionCodeDownloadInvalidMetadataSize";
    case kActionCodeDownloadInvalidMetadataSignature:
      return "kActionCodeDownloadInvalidMetadataSignature";
    case kActionCodeOmahaResponseInvalid:
      return "kActionCodeOmahaResponseInvalid";
    case kActionCodeOmahaUpdateIgnoredPerPolicy:
      return "kActionCodeOmahaUpdateIgnoredPerPolicy";
    case kActionCodeOmahaUpdateDeferredPerPolicy:
      return "kActionCodeOmahaUpdateDeferredPerPolicy";
    case kActionCodeOmahaErrorInHTTPResponse:
      return "kActionCodeOmahaErrorInHTTPResponse";
    case kActionCodeDownloadOperationHashMissingError:
      return "kActionCodeDownloadOperationHashMissingError";
    case kActionCodeDownloadMetadataSignatureMissingError:
      return "kActionCodeDownloadMetadataSignatureMissingError";
    case kActionCodeOmahaUpdateDeferredForBackoff:
      return "kActionCodeOmahaUpdateDeferredForBackoff";
    case kActionCodePostinstallPowerwashError:
      return "kActionCodePostinstallPowerwashError";
    case kActionCodeUmaReportedMax:
      return "kActionCodeUmaReportedMax";
    case kActionCodeOmahaRequestHTTPResponseBase:
      return "kActionCodeOmahaRequestHTTPResponseBase";
    case kActionCodeResumedFlag:
      return "Resumed";
    case kActionCodeDevModeFlag:
      return "DevMode";
    case kActionCodeTestImageFlag:
      return "TestImage";
    case kActionCodeTestOmahaUrlFlag:
      return "TestOmahaUrl";
    case kSpecialFlags:
      return "kSpecialFlags";
    // Don't add a default case to let the compiler warn about newly added
    // error codes which should be added here.
  }

  return "Unknown error: " + base::UintToString(static_cast<unsigned>(code));
}

}  // namespace utils

}  // namespace chromeos_update_engine
