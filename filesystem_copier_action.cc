// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
const off_t kCopyFileBufferSize = 2 * 1024 * 1024;
}  // namespace {}

void FilesystemCopierAction::PerformAction() {
  // Will tell the ActionProcessor we've failed if we return.
  ScopedActionCompleter abort_action_completer(processor_, this);

  if (!HasInputObject()) {
    LOG(ERROR) << "FilesystemCopierAction missing input object.";
    return;
  }
  install_plan_ = GetInputObject();

  if (install_plan_.is_full_update) {
    // No copy needed. Done!
    abort_action_completer.set_success(true);
    return;
  }

  const string source =
      copy_source_.empty() ? utils::BootDevice() : copy_source_;
  LOG(INFO) << "Copying from " << source << " to "
            << install_plan_.install_path;
  
  int src_fd = open(source.c_str(), O_RDONLY);
  if (src_fd < 0) {
    PLOG(ERROR) << "Unable to open " << source << " for reading:";
    return;
  }
  int dst_fd = open(install_plan_.install_path.c_str(),
                    O_WRONLY | O_TRUNC | O_CREAT,
                    0644);
  if (dst_fd < 0) {
    close(src_fd);
    PLOG(ERROR) << "Unable to open " << install_plan_.install_path
                << " for writing:";
    return;
  }

  src_stream_ = g_unix_input_stream_new(src_fd, TRUE);
  dst_stream_ = g_unix_output_stream_new(dst_fd, TRUE);

  buffer_.resize(kCopyFileBufferSize);

  // Set up the first read
  canceller_ = g_cancellable_new();

  g_input_stream_read_async(src_stream_,
                            &buffer_[0],
                            buffer_.size(),
                            G_PRIORITY_DEFAULT,
                            canceller_,
                            &FilesystemCopierAction::StaticAsyncReadyCallback,
                            this);
  read_in_flight_ = true;

  abort_action_completer.set_should_complete(false);
}

void FilesystemCopierAction::TerminateProcessing() {
  if (canceller_) {
    g_cancellable_cancel(canceller_);
  }
}

void FilesystemCopierAction::Cleanup(bool success, bool was_cancelled) {
  g_object_unref(src_stream_);
  src_stream_ = NULL;
  g_object_unref(dst_stream_);
  dst_stream_ = NULL;
  if (was_cancelled)
    return;
  if (success && HasOutputPipe())
    SetOutputObject(install_plan_);
  processor_->ActionComplete(this, success);
}

void FilesystemCopierAction::AsyncReadyCallback(GObject *source_object,
                                                GAsyncResult *res) {
  GError* error = NULL;
  CHECK(canceller_);
  bool was_cancelled = g_cancellable_is_cancelled(canceller_) == TRUE;
  g_object_unref(canceller_);
  canceller_ = NULL;

  if (read_in_flight_) {
    ssize_t bytes_read = g_input_stream_read_finish(src_stream_, res, &error);
    if (bytes_read < 0) {
      LOG(ERROR) << "Read failed:" << utils::GetGErrorMessage(error);
      Cleanup(false, was_cancelled);
      return;
    }

    if (bytes_read == 0) {
      // We're done!
      Cleanup(true, was_cancelled);
      return;
    }
    // Kick off a write
    read_in_flight_ = false;
    buffer_valid_size_ = bytes_read;
    canceller_ = g_cancellable_new();
    g_output_stream_write_async(
        dst_stream_,
        &buffer_[0],
        bytes_read,
        G_PRIORITY_DEFAULT,
        canceller_,
        &FilesystemCopierAction::StaticAsyncReadyCallback,
        this);
    return;
  }

  ssize_t bytes_written = g_output_stream_write_finish(dst_stream_,
                                                       res,
                                                       &error);
  if (bytes_written < static_cast<ssize_t>(buffer_valid_size_)) {
    if (bytes_written < 0) {
      LOG(ERROR) << "Write failed:" << utils::GetGErrorMessage(error);
    } else {
      LOG(ERROR) << "Write was short: wrote " << bytes_written
                 << " but expected to write " << buffer_valid_size_;
    }
    Cleanup(false, was_cancelled);
    return;
  }

  // Kick off a read
  read_in_flight_ = true;
  canceller_ = g_cancellable_new();
  g_input_stream_read_async(
      src_stream_,
      &buffer_[0],
      buffer_.size(),
      G_PRIORITY_DEFAULT,
      canceller_,
      &FilesystemCopierAction::StaticAsyncReadyCallback,
      this);
}

}  // namespace chromeos_update_engine
