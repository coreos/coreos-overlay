// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/filesystem_copier_action.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include "update_engine/filesystem_iterator.h"
#include "update_engine/subprocess.h"
#include "update_engine/utils.h"

using std::map;
using std::min;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
const char* kMountpointTemplate = "/tmp/au_dest_mnt.XXXXXX";
const off_t kCopyFileBufferSize = 4 * 1024 * 1024;
const char* kCopyExclusionPrefix = "/lost+found";
}  // namespace {}

void FilesystemCopierAction::PerformAction() {
  if (!HasInputObject()) {
    LOG(ERROR) << "No input object. Aborting.";
    processor_->ActionComplete(this, false);
    return;
  }
  install_plan_ = GetInputObject();

  if (install_plan_.is_full_update) {
    // No copy needed.
    processor_->ActionComplete(this, true);
    return;
  }

  {
    // Set up dest_path_
    char *dest_path_temp = strdup(kMountpointTemplate);
    CHECK(dest_path_temp);
    CHECK_EQ(mkdtemp(dest_path_temp), dest_path_temp);
    CHECK_NE(dest_path_temp[0], '\0');
    dest_path_ = dest_path_temp;
    free(dest_path_temp);
  }

  // Make sure we're properly mounted
  if (Mount(install_plan_.install_path, dest_path_)) {
    bool done_early = false;
    if (utils::FileExists(
            (dest_path_ +
             FilesystemCopierAction::kCompleteFilesystemMarker).c_str())) {
      // We're done!
      done_early = true;
      skipped_copy_ = true;
      if (HasOutputPipe())
        SetOutputObject(install_plan_);
    }
    if (!Unmount(dest_path_)) {
      LOG(ERROR) << "Unmount failed. Aborting.";
      processor_->ActionComplete(this, false);
      return;
    }
    if (done_early) {
      CHECK(!is_mounted_);
      if (rmdir(dest_path_.c_str()) != 0)
        LOG(ERROR) << "Unable to remove " << dest_path_;
      processor_->ActionComplete(this, true);
      return;
    }
  }
  LOG(ERROR) << "not mounted; spawning thread";
  // If we get here, mount failed or we're not done yet. Reformat and copy.
  CHECK_EQ(pthread_create(&helper_thread_, NULL, HelperThreadMainStatic, this),
           0);
}

void FilesystemCopierAction::TerminateProcessing() {
  if (is_mounted_) {
    LOG(ERROR) << "Aborted processing, but left a filesystem mounted.";
  }
}

bool FilesystemCopierAction::Mount(const string& device,
                                   const string& mountpoint) {
  CHECK(!is_mounted_);
  if(utils::MountFilesystem(device, mountpoint))
    is_mounted_ = true;
  return is_mounted_;
}

bool FilesystemCopierAction::Unmount(const string& mountpoint) {
  CHECK(is_mounted_);
  if (utils::UnmountFilesystem(mountpoint))
    is_mounted_ = false;
  return !is_mounted_;
}

void* FilesystemCopierAction::HelperThreadMain() {
  // First, format the drive
  vector<string> cmd;
  cmd.push_back("/sbin/mkfs.ext3");
  cmd.push_back("-F");
  cmd.push_back(install_plan_.install_path);
  int return_code = 1;
  bool success = Subprocess::SynchronousExec(cmd, &return_code);
  if (return_code != 0) {
    LOG(INFO) << "Format of " << install_plan_.install_path
              << " failed. Exit code: " << return_code;
    success = false;
  }
  if (success) {
    if (!Mount(install_plan_.install_path, dest_path_)) {
      LOG(ERROR) << "Mount failed. Aborting";
      success = false;
    }
  }
  if (success) {
    success = CopySynchronously();
  }
  if (success) {
    // Place our marker to avoid copies again in the future
    int r = open((dest_path_ +
                  FilesystemCopierAction::kCompleteFilesystemMarker).c_str(),
                 O_CREAT | O_WRONLY, 0644);
    if (r >= 0)
      close(r);
  }
  // Unmount
  if (!Unmount(dest_path_)) {
    LOG(ERROR) << "Unmount failed. Aborting";
    success = false;
  }
  if (HasOutputPipe())
    SetOutputObject(install_plan_);

  // Tell main thread that we're done
  g_timeout_add(0, CollectThreadStatic, this);
  return reinterpret_cast<void*>(success ? 0 : 1);
}

void FilesystemCopierAction::CollectThread() {
  void *thread_ret_value = NULL;
  CHECK_EQ(pthread_join(helper_thread_, &thread_ret_value), 0);
  bool success = (thread_ret_value == 0);
  CHECK(!is_mounted_);
  if (rmdir(dest_path_.c_str()) != 0)
    LOG(INFO) << "Unable to remove " << dest_path_;
  LOG(INFO) << "FilesystemCopierAction done";
  processor_->ActionComplete(this, success);
}

bool FilesystemCopierAction::CreateDirSynchronously(const std::string& new_path,
                                                    const struct stat& stbuf) {
  int r = mkdir(new_path.c_str(), stbuf.st_mode);
  TEST_AND_RETURN_FALSE_ERRNO(r == 0);
  return true;
}

bool FilesystemCopierAction::CopyFileSynchronously(const std::string& old_path,
                                                   const std::string& new_path,
                                                   const struct stat& stbuf) {
  int fd_out = open(new_path.c_str(), O_CREAT | O_EXCL | O_WRONLY,
                    stbuf.st_mode);
  TEST_AND_RETURN_FALSE_ERRNO(fd_out >= 0);
  ScopedFdCloser fd_out_closer(&fd_out);
  int fd_in = open(old_path.c_str(), O_RDONLY, 0);
  TEST_AND_RETURN_FALSE_ERRNO(fd_in >= 0);
  ScopedFdCloser fd_in_closer(&fd_in);

  vector<char> buf(min(kCopyFileBufferSize, stbuf.st_size));
  off_t bytes_written = 0;
  while (true) {
    // Make sure we don't need to abort early:
    TEST_AND_RETURN_FALSE(!g_atomic_int_get(&thread_should_exit_));

    ssize_t read_size = read(fd_in, &buf[0], buf.size());
    TEST_AND_RETURN_FALSE_ERRNO(read_size >= 0);
    if (0 == read_size)  // EOF
      break;

    ssize_t write_size = 0;
    while (write_size < read_size) {
      ssize_t r = write(fd_out, &buf[write_size], read_size - write_size);
      TEST_AND_RETURN_FALSE_ERRNO(r >= 0);
      write_size += r;
    }
    CHECK_EQ(write_size, read_size);
    bytes_written += write_size;
    CHECK_LE(bytes_written, stbuf.st_size);
    if (bytes_written == stbuf.st_size)
      break;
  }
  CHECK_EQ(bytes_written, stbuf.st_size);
  return true;
}

bool FilesystemCopierAction::CreateHardLinkSynchronously(
    const std::string& old_path,
    const std::string& new_path) {
  int r = link(old_path.c_str(), new_path.c_str());
  TEST_AND_RETURN_FALSE_ERRNO(r == 0);
  return true;
}

bool FilesystemCopierAction::CopySymlinkSynchronously(
    const std::string& old_path,
    const std::string& new_path,
    const struct stat& stbuf) {
  vector<char> buf(PATH_MAX + 1);
  ssize_t r = readlink(old_path.c_str(), &buf[0], buf.size());
  TEST_AND_RETURN_FALSE_ERRNO(r >= 0);
  // Make sure we got the entire link
  TEST_AND_RETURN_FALSE(static_cast<unsigned>(r) < buf.size());
  buf[r] = '\0';
  int rc = symlink(&buf[0], new_path.c_str());
  TEST_AND_RETURN_FALSE_ERRNO(rc == 0);
  return true;
}

bool FilesystemCopierAction::CreateNodeSynchronously(
    const std::string& new_path,
    const struct stat& stbuf) {
  int r = mknod(new_path.c_str(), stbuf.st_mode, stbuf.st_rdev);
  TEST_AND_RETURN_FALSE_ERRNO(r == 0);
  return true;
}

// Returns true on success
bool FilesystemCopierAction::CopySynchronously() {
  // This map is a map from inode # to new_path.
  map<ino_t, string> hard_links;
  FilesystemIterator iter(copy_source_,
                          utils::SetWithValue<string>(kCopyExclusionPrefix));
  bool success = true;
  for (; !g_atomic_int_get(&thread_should_exit_) &&
           !iter.IsEnd(); iter.Increment()) {
    const string old_path = iter.GetFullPath();
    const string new_path = dest_path_ + iter.GetPartialPath();
    LOG(INFO) << "copying " << old_path << " to " << new_path;
    const struct stat stbuf = iter.GetStat();
    success = false;

    // Skip lost+found
    CHECK_NE(kCopyExclusionPrefix, iter.GetPartialPath());

    // Directories can't be hard-linked, so check for directories first
    if (iter.GetPartialPath().empty()) {
      // Root has an empty path.
      // We don't need to create anything for the root, which is the first
      // thing we get from the iterator.
      success = true;
    } else if (S_ISDIR(stbuf.st_mode)) {
      success = CreateDirSynchronously(new_path, stbuf);
    } else {
      if (stbuf.st_nlink > 1 &&
          utils::MapContainsKey(hard_links, stbuf.st_ino)) {
        success = CreateHardLinkSynchronously(hard_links[stbuf.st_ino],
                                              new_path);
      } else {
        if (stbuf.st_nlink > 1)
          hard_links[stbuf.st_ino] = new_path;
        if (S_ISREG(stbuf.st_mode)) {
          success = CopyFileSynchronously(old_path, new_path, stbuf);
        } else if (S_ISLNK(stbuf.st_mode)) {
          success = CopySymlinkSynchronously(old_path, new_path, stbuf);
        } else if (S_ISFIFO(stbuf.st_mode) ||
                   S_ISCHR(stbuf.st_mode) ||
                   S_ISBLK(stbuf.st_mode) ||
                   S_ISSOCK(stbuf.st_mode)) {
          success = CreateNodeSynchronously(new_path, stbuf);
        } else {
          CHECK(false) << "Unable to copy file " << old_path << " with mode "
                       << stbuf.st_mode;
        }
      }
    }
    TEST_AND_RETURN_FALSE(success);

    // chmod new file
    if (!S_ISLNK(stbuf.st_mode)) {
      int r = chmod(new_path.c_str(), stbuf.st_mode);
      TEST_AND_RETURN_FALSE_ERRNO(r == 0);
    }

    // Set uid/gid.
    int r = lchown(new_path.c_str(), stbuf.st_uid, stbuf.st_gid);
    TEST_AND_RETURN_FALSE_ERRNO(r == 0);
  }
  TEST_AND_RETURN_FALSE(!iter.IsErr());
  // Success!
  return true;
}

const char* FilesystemCopierAction::kCompleteFilesystemMarker(
    "/update_engine_copy_success");

}  // namespace chromeos_update_engine
