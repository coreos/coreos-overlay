// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/filesystem_copier_action.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <glib.h>

#include "update_engine/filesystem_iterator.h"
#include "update_engine/subprocess.h"
#include "update_engine/utils.h"

using std::map;
using std::min;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
const off_t kCopyFileBufferSize = 128 * 1024;
}  // namespace {}

FilesystemCopierAction::FilesystemCopierAction(bool verify_hash)
    : verify_hash_(verify_hash),
      src_stream_(NULL),
      dst_stream_(NULL),
      read_done_(false),
      failed_(false),
      cancelled_(false),
      filesystem_size_(std::numeric_limits<int64_t>::max()) {
  // A lot of code works on the implicit assumption that processing is done on
  // exactly 2 ping-pong buffers.
  static_assert(arraysize(buffer_) == 2 &&
                arraysize(buffer_state_) == 2 &&
                arraysize(buffer_valid_size_) == 2 &&
                arraysize(canceller_) == 2,
                "ping-pong buffers not two");
  for (int i = 0; i < 2; ++i) {
    buffer_state_[i] = kBufferStateEmpty;
    buffer_valid_size_[i] = 0;
    canceller_[i] = NULL;
  }
}

void FilesystemCopierAction::PerformAction() {
  // Will tell the ActionProcessor we've failed if we return.
  ScopedActionCompleter abort_action_completer(processor_, this);

  if (!HasInputObject()) {
    LOG(ERROR) << "FilesystemCopierAction missing input object.";
    return;
  }
  install_plan_ = GetInputObject();
  if (!verify_hash_ && install_plan_.is_resume) {
    // No copy or hash verification needed. Done!
    if (HasOutputPipe())
      SetOutputObject(install_plan_);
    abort_action_completer.set_code(kActionCodeSuccess);
    return;
  }

  const string source = verify_hash_ ?
      install_plan_.partition_path : install_plan_.old_partition_path;
  int src_fd = open(source.c_str(), O_RDONLY);
  if (src_fd < 0) {
    PLOG(ERROR) << "Unable to open " << source << " for reading:";
    return;
  }

  if (!verify_hash_) {
    int dst_fd = open(install_plan_.partition_path.c_str(),
                      O_WRONLY | O_TRUNC | O_CREAT,
                    0644);
    if (dst_fd < 0) {
      close(src_fd);
      PLOG(ERROR) << "Unable to open " << install_plan_.partition_path
                  << " for writing:";
      return;
    }

    dst_stream_ = g_unix_output_stream_new(dst_fd, TRUE);
  }

  DetermineFilesystemSize(src_fd);
  src_stream_ = g_unix_input_stream_new(src_fd, TRUE);

  for (int i = 0; i < 2; i++) {
    buffer_[i].resize(kCopyFileBufferSize);
    canceller_[i] = g_cancellable_new();
  }

  // Start the first read.
  SpawnAsyncActions();

  abort_action_completer.set_should_complete(false);
}

void FilesystemCopierAction::TerminateProcessing() {
  for (int i = 0; i < 2; i++) {
    if (canceller_[i]) {
      g_cancellable_cancel(canceller_[i]);
    }
  }
}

bool FilesystemCopierAction::IsCleanupPending() const {
  return (src_stream_ != NULL);
}

void FilesystemCopierAction::Cleanup(ActionExitCode code) {
  for (int i = 0; i < 2; i++) {
    g_object_unref(canceller_[i]);
    canceller_[i] = NULL;
  }
  g_object_unref(src_stream_);
  src_stream_ = NULL;
  if (dst_stream_) {
    g_object_unref(dst_stream_);
    dst_stream_ = NULL;
  }
  if (cancelled_)
    return;
  if (code == kActionCodeSuccess && HasOutputPipe())
    SetOutputObject(install_plan_);
  processor_->ActionComplete(this, code);
}

void FilesystemCopierAction::AsyncReadReadyCallback(GObject *source_object,
                                                    GAsyncResult *res) {
  int index = buffer_state_[0] == kBufferStateReading ? 0 : 1;
  CHECK(buffer_state_[index] == kBufferStateReading);

  GError* error = NULL;
  CHECK(canceller_[index]);
  cancelled_ = g_cancellable_is_cancelled(canceller_[index]) == TRUE;

  ssize_t bytes_read = g_input_stream_read_finish(src_stream_, res, &error);
  if (bytes_read < 0) {
    LOG(ERROR) << "Read failed: " << utils::GetAndFreeGError(&error);
    failed_ = true;
    buffer_state_[index] = kBufferStateEmpty;
  } else if (bytes_read == 0) {
    read_done_ = true;
    buffer_state_[index] = kBufferStateEmpty;
  } else {
    buffer_valid_size_[index] = bytes_read;
    buffer_state_[index] = kBufferStateFull;
    filesystem_size_ -= bytes_read;
  }
  SpawnAsyncActions();

  if (bytes_read > 0) {
    // If read_done_ is set, SpawnAsyncActions may finalize the hash so the hash
    // update below would happen too late.
    CHECK(!read_done_);
    if (!hasher_.Update(buffer_[index].data(), bytes_read)) {
      LOG(ERROR) << "Unable to update the hash.";
      failed_ = true;
    }
    if (verify_hash_) {
      buffer_state_[index] = kBufferStateEmpty;
    }
  }
}

void FilesystemCopierAction::StaticAsyncReadReadyCallback(
    GObject *source_object,
    GAsyncResult *res,
    gpointer user_data) {
  reinterpret_cast<FilesystemCopierAction*>(user_data)->
      AsyncReadReadyCallback(source_object, res);
}

void FilesystemCopierAction::AsyncWriteReadyCallback(GObject *source_object,
                                                     GAsyncResult *res) {
  int index = buffer_state_[0] == kBufferStateWriting ? 0 : 1;
  CHECK(buffer_state_[index] == kBufferStateWriting);
  buffer_state_[index] = kBufferStateEmpty;

  GError* error = NULL;
  CHECK(canceller_[index]);
  cancelled_ = g_cancellable_is_cancelled(canceller_[index]) == TRUE;

  ssize_t bytes_written = g_output_stream_write_finish(dst_stream_,
                                                       res,
                                                       &error);

  if (bytes_written < static_cast<ssize_t>(buffer_valid_size_[index])) {
    if (bytes_written < 0) {
      LOG(ERROR) << "Write error: " << utils::GetAndFreeGError(&error);
    } else {
      LOG(ERROR) << "Wrote too few bytes: " << bytes_written
                 << " < " << buffer_valid_size_[index];
    }
    failed_ = true;
  }

  SpawnAsyncActions();
}

void FilesystemCopierAction::StaticAsyncWriteReadyCallback(
    GObject *source_object,
    GAsyncResult *res,
    gpointer user_data) {
  reinterpret_cast<FilesystemCopierAction*>(user_data)->
      AsyncWriteReadyCallback(source_object, res);
}

void FilesystemCopierAction::SpawnAsyncActions() {
  bool reading = false;
  bool writing = false;
  for (int i = 0; i < 2; i++) {
    if (buffer_state_[i] == kBufferStateReading) {
      reading = true;
    }
    if (buffer_state_[i] == kBufferStateWriting) {
      writing = true;
    }
  }
  if (failed_ || cancelled_) {
    if (!reading && !writing) {
      Cleanup(kActionCodeError);
    }
    return;
  }
  for (int i = 0; i < 2; i++) {
    if (!reading && !read_done_ && buffer_state_[i] == kBufferStateEmpty) {
      int64_t bytes_to_read = std::min(static_cast<int64_t>(buffer_[0].size()),
                                       filesystem_size_);
      g_input_stream_read_async(
          src_stream_,
          buffer_[i].data(),
          bytes_to_read,
          G_PRIORITY_DEFAULT,
          canceller_[i],
          &FilesystemCopierAction::StaticAsyncReadReadyCallback,
          this);
      reading = true;
      buffer_state_[i] = kBufferStateReading;
    } else if (!writing && !verify_hash_ &&
               buffer_state_[i] == kBufferStateFull) {
      g_output_stream_write_async(
          dst_stream_,
          buffer_[i].data(),
          buffer_valid_size_[i],
          G_PRIORITY_DEFAULT,
          canceller_[i],
          &FilesystemCopierAction::StaticAsyncWriteReadyCallback,
          this);
      writing = true;
      buffer_state_[i] = kBufferStateWriting;
    }
  }
  if (!reading && !writing) {
    // We're done!
    ActionExitCode code = kActionCodeSuccess;
    if (hasher_.Finalize()) {
      LOG(INFO) << "Hash: " << hasher_.hash();
      if (verify_hash_) {
        if (install_plan_.new_partition_hash != hasher_.raw_hash()) {
          code = kActionCodeNewRootfsVerificationError;
          LOG(ERROR) << "New partition verification failed.";
        }
      } else {
        install_plan_.old_partition_hash = hasher_.raw_hash();
      }
    } else {
      LOG(ERROR) << "Unable to finalize the hash.";
      code = kActionCodeError;
    }
    Cleanup(code);
  }
}

void FilesystemCopierAction::DetermineFilesystemSize(int fd) {
  if (verify_hash_) {
    filesystem_size_ = install_plan_.new_partition_size;
    LOG(INFO) << "Filesystem size: " << filesystem_size_;
  } else if (utils::GetDeviceSizeFromFD(fd, &filesystem_size_)) {
    LOG(INFO) << "Filesystem size: " << filesystem_size_;
  } else {
    filesystem_size_ = std::numeric_limits<int64_t>::max();
  }
}

}  // namespace chromeos_update_engine
