// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_FILESYSTEM_COPIER_ACTION_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_FILESYSTEM_COPIER_ACTION_H__

#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include <gio/gio.h>
#include <glib.h>
#include "update_engine/action.h"
#include "update_engine/install_plan.h"

// This action will only do real work if it's a delta update. It will
// copy the root partition to install partition, and then terminate.

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
  explicit FilesystemCopierAction(bool copying_kernel_install_path)
      : copying_kernel_install_path_(copying_kernel_install_path),
        src_stream_(NULL),
        dst_stream_(NULL),
        canceller_(NULL),
        read_in_flight_(false),
        buffer_valid_size_(0) {}
  typedef ActionTraits<FilesystemCopierAction>::InputObjectType
  InputObjectType;
  typedef ActionTraits<FilesystemCopierAction>::OutputObjectType
  OutputObjectType;
  void PerformAction();
  void TerminateProcessing();

  // Used for testing, so we can copy from somewhere other than root
  void set_copy_source(const std::string& path) {
    copy_source_ = path;
  }

  // Debugging/logging
  static std::string StaticType() { return "FilesystemCopierAction"; }
  std::string Type() const { return StaticType(); }

 private:
  // Callback from glib when the copy operation is done.
  void AsyncReadyCallback(GObject *source_object, GAsyncResult *res);
  static void StaticAsyncReadyCallback(GObject *source_object,
                                       GAsyncResult *res,
                                       gpointer user_data) {
    reinterpret_cast<FilesystemCopierAction*>(user_data)->AsyncReadyCallback(
        source_object, res);
  }
  
  // Cleans up all the variables we use for async operations and tells
  // the ActionProcessor we're done w/ success as passed in.
  // was_cancelled should be true if TerminateProcessing() was called.
  void Cleanup(bool success, bool was_cancelled);
  
  // If true, this action is copying to the kernel_install_path from
  // the install plan, otherwise it's copying just to the install_path.
  const bool copying_kernel_install_path_;
  
  // The path to copy from. If empty (the default), the source is from the
  // passed in InstallPlan.
  std::string copy_source_;

  // If non-NULL, these are GUnixInputStream objects for the opened
  // source/destination partitions.
  GInputStream* src_stream_;
  GOutputStream* dst_stream_;
  
  // If non-NULL, the cancellable object for the in-flight async call.
  GCancellable* canceller_;
  
  // True if we're waiting on a read to complete; false if we're
  // waiting on a write.
  bool read_in_flight_;
  
  // The buffer for storing data we read/write.
  std::vector<char> buffer_;

  // Number of valid elements in buffer_.
  std::vector<char>::size_type buffer_valid_size_;

  // The install plan we're passed in via the input pipe.
  InstallPlan install_plan_;
  
  DISALLOW_COPY_AND_ASSIGN(FilesystemCopierAction);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_FILESYSTEM_COPIER_ACTION_H__
