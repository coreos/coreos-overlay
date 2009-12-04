// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_FILESYSTEM_COPIER_ACTION_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_FILESYSTEM_COPIER_ACTION_H__

#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <glib.h>
#include "update_engine/action.h"
#include "update_engine/install_plan.h"

// This action will only do real work if it's a delta update. It will
// format the install partition as ext3/4, copy the root filesystem into it,
// and then terminate.

// Implementation notes: This action uses a helper thread, which seems to
// violate the design decision to only have a single thread and use
// asynchronous i/o. The issue is that (to the best of my knowledge),
// there are no linux APIs to crawl a filesystem's metadata asynchronously.
// The suggested way seems to be to open the raw device and parse the ext
// filesystem. That's not a good approach for a number of reasons:
// - ties us to ext filesystem
// - although this wouldn't happen at the time of writing, it may not handle
//   changes to the source fs during the copy as gracefully.
// - requires us to have read-access to the source filesystem device, which
//   may be a security issue.
//
// Having said this, using a helper thread is not ideal, but it's acceptable:
// we still honor the Action API. That is, all interaction between the action
// and other objects in the system (e.g. the ActionProcessor) happens on the
// main thread. The helper thread is fully encapsulated by the action.

namespace chromeos_update_engine {

class FilesystemCopierAction;

template<>
class ActionTraits<FilesystemCopierAction> {
 public:
  // Takes the install plan as input
  typedef InstallPlan InputObjectType;
  // Passes the install plan as output
  typedef InstallPlan OutputObjectType;
};

class FilesystemCopierAction : public Action<FilesystemCopierAction> {
 public:
  FilesystemCopierAction()
      : thread_should_exit_(0),
        is_mounted_(false),
        copy_source_("/"),
        skipped_copy_(false) {}
  typedef ActionTraits<FilesystemCopierAction>::InputObjectType
  InputObjectType;
  typedef ActionTraits<FilesystemCopierAction>::OutputObjectType
  OutputObjectType;
  void PerformAction();
  void TerminateProcessing();

  // Used for testing, so we can copy from somewhere other than root
  void set_copy_source(const string& path) {
    copy_source_ = path;
  }
  // Returns true if we detected that a copy was unneeded and thus skipped it.
  bool skipped_copy() { return skipped_copy_; }

  // Debugging/logging
  static std::string StaticType() { return "FilesystemCopierAction"; }
  std::string Type() const { return StaticType(); }

 private:
  // These synchronously mount or unmount the given mountpoint
  bool Mount(const string& device, const string& mountpoint);
  bool Unmount(const string& mountpoint);

  // Performs a recursive file/directory copy from copy_source_ to dest_path_.
  // Doesn't return until the copy has completed. Returns true on success
  // or false on error.
  bool CopySynchronously();

  // There are helper functions for CopySynchronously. They handle creating
  // various types of files. They return true on success.
  bool CreateDirSynchronously(const std::string& new_path,
                              const struct stat& stbuf);
  bool CopyFileSynchronously(const std::string& old_path,
                             const std::string& new_path,
                             const struct stat& stbuf);
  bool CreateHardLinkSynchronously(const std::string& old_path,
                                   const std::string& new_path);
  // Note: Here, old_path is an existing symlink that will be copied to
  // new_path. Thus, old_path is *not* the same as the old_path from
  // the symlink() syscall.
  bool CopySymlinkSynchronously(const std::string& old_path,
                                const std::string& new_path,
                                const struct stat& stbuf);
  bool CreateNodeSynchronously(const std::string& new_path,
                               const struct stat& stbuf);

  // Returns NULL on success
  void* HelperThreadMain();
  static void* HelperThreadMainStatic(void* data) {
    FilesystemCopierAction* self =
        reinterpret_cast<FilesystemCopierAction*>(data);
    return self->HelperThreadMain();
  }

  // Joins the thread and tells the processor that we're done
  void CollectThread();
  // GMainLoop callback function:
  static gboolean CollectThreadStatic(gpointer data) {
    FilesystemCopierAction* self =
        reinterpret_cast<FilesystemCopierAction*>(data);
    self->CollectThread();
    return FALSE;
  }

  pthread_t helper_thread_;

  volatile gint thread_should_exit_;

  static const char* kCompleteFilesystemMarker;

  // Whether or not the destination device is currently mounted.
  bool is_mounted_;

  // Where the destination device is mounted.
  string dest_path_;

  // The path to copy from. Usually left as the default "/", but tests can
  // change it.
  string copy_source_;

  // The install plan we're passed in via the input pipe.
  InstallPlan install_plan_;

  // Set to true if we detected the copy was unneeded and thus we skipped it.
  bool skipped_copy_;

  DISALLOW_COPY_AND_ASSIGN(FilesystemCopierAction);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_FILESYSTEM_COPIER_ACTION_H__
