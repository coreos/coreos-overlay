// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/action.h"
#include "update_engine/install_plan.h"
#include "update_engine/omaha_hash_calculator.h"

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
  FilesystemCopierAction(bool verify_hash);

  typedef ActionTraits<FilesystemCopierAction>::InputObjectType
  InputObjectType;
  typedef ActionTraits<FilesystemCopierAction>::OutputObjectType
  OutputObjectType;
  void PerformAction();
  void TerminateProcessing();

  // Used for testing. Return true if Cleanup() has not yet been called due
  // to a callback upon the completion or cancellation of the copier action.
  // A test should wait until IsCleanupPending() returns false before
  // terminating the glib main loop.
  bool IsCleanupPending() const;

  // Used for testing, so we can copy from somewhere other than root
  void set_copy_source(const std::string& path) { copy_source_ = path; }

  // Debugging/logging
  static std::string StaticType() { return "FilesystemCopierAction"; }
  std::string Type() const { return StaticType(); }

 private:
  friend class FilesystemCopierActionTest;
  FRIEND_TEST(FilesystemCopierActionTest, DetermineFilesystemSizeTest);

  // Ping-pong buffers generally cycle through the following states:
  // Empty->Reading->Full->Writing->Empty. In hash verification mode the state
  // is never set to Writing.
  enum BufferState {
    kBufferStateEmpty,
    kBufferStateReading,
    kBufferStateFull,
    kBufferStateWriting
  };

  // Callbacks from glib when the read/write operation is done.
  void AsyncReadReadyCallback(GObject *source_object, GAsyncResult *res);
  static void StaticAsyncReadReadyCallback(GObject *source_object,
                                           GAsyncResult *res,
                                           gpointer user_data);

  void AsyncWriteReadyCallback(GObject *source_object, GAsyncResult *res);
  static void StaticAsyncWriteReadyCallback(GObject *source_object,
                                            GAsyncResult *res,
                                            gpointer user_data);

  // Based on the state of the ping-pong buffers spawns appropriate read/write
  // actions asynchronously.
  void SpawnAsyncActions();

  // Cleans up all the variables we use for async operations and tells the
  // ActionProcessor we're done w/ |code| as passed in. |cancelled_| should be
  // true if TerminateProcessing() was called.
  void Cleanup(ActionExitCode code);

  // Determine, if possible, the source file system size to avoid copying the
  // whole partition. Currently this supports only the root file system assuming
  // it's ext3-compatible.
  void DetermineFilesystemSize(int fd);

  // If true, this action is running in applied update hash verification mode --
  // it computes a hash for the target install path and compares it against the
  // expected value.
  const bool verify_hash_;

  // The path to copy from. If empty (the default), the source is from the
  // passed in InstallPlan.
  std::string copy_source_;

  // If non-NULL, these are GUnixInputStream objects for the opened
  // source/destination partitions.
  GInputStream* src_stream_;
  GOutputStream* dst_stream_;

  // Ping-pong buffers for storing data we read/write. Only one buffer is being
  // read at a time and only one buffer is being written at a time.
  std::vector<char> buffer_[2];

  // The state of each buffer.
  BufferState buffer_state_[2];

  // Number of valid elements in |buffer_| if its state is kBufferStateFull.
  std::vector<char>::size_type buffer_valid_size_[2];

  // The cancellable objects for the in-flight async calls.
  GCancellable* canceller_[2];

  bool read_done_;  // true if reached EOF on the input stream.
  bool failed_;  // true if the action has failed.
  bool cancelled_;  // true if the action has been cancelled.

  // The install plan we're passed in via the input pipe.
  InstallPlan install_plan_;

  // Calculates the hash of the copied data.
  OmahaHashCalculator hasher_;

  // Copies and hashes this many bytes from the head of the input stream. This
  // field is initialized when the action is started and decremented as more
  // bytes get copied.
  int64_t filesystem_size_;

  DISALLOW_COPY_AND_ASSIGN(FilesystemCopierAction);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_FILESYSTEM_COPIER_ACTION_H__
