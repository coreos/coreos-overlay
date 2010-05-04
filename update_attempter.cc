// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_attempter.h"
#include <tr1/memory>
#include <string>
#include <vector>
#include <glib.h>
#include "update_engine/download_action.h"
#include "update_engine/filesystem_copier_action.h"
#include "update_engine/libcurl_http_fetcher.h"
#include "update_engine/omaha_request_prep_action.h"
#include "update_engine/omaha_response_handler_action.h"
#include "update_engine/postinstall_runner_action.h"
#include "update_engine/set_bootable_flag_action.h"
#include "update_engine/update_check_action.h"

using std::tr1::shared_ptr;
using std::string;
using std::vector;

namespace chromeos_update_engine {

void UpdateAttempter::Update(bool force_full_update) {
  full_update_ = force_full_update;
  CHECK(!processor_.IsRunning());
  processor_.set_delegate(this);

  // Actions:
  shared_ptr<OmahaRequestPrepAction> request_prep_action(
      new OmahaRequestPrepAction(force_full_update));
  shared_ptr<UpdateCheckAction> update_check_action(
      new UpdateCheckAction(new LibcurlHttpFetcher));
  shared_ptr<OmahaResponseHandlerAction> response_handler_action(
      new OmahaResponseHandlerAction);
  shared_ptr<FilesystemCopierAction> filesystem_copier_action(
      new FilesystemCopierAction(false));
  shared_ptr<FilesystemCopierAction> kernel_filesystem_copier_action(
      new FilesystemCopierAction(true));
  shared_ptr<DownloadAction> download_action(
      new DownloadAction(new LibcurlHttpFetcher));
  shared_ptr<PostinstallRunnerAction> postinstall_runner_action_precommit(
      new PostinstallRunnerAction(true));
  shared_ptr<SetBootableFlagAction> set_bootable_flag_action(
      new SetBootableFlagAction);
  shared_ptr<PostinstallRunnerAction> postinstall_runner_action_postcommit(
      new PostinstallRunnerAction(false));
      
  response_handler_action_ = response_handler_action;

  actions_.push_back(shared_ptr<AbstractAction>(request_prep_action));
  actions_.push_back(shared_ptr<AbstractAction>(update_check_action));
  actions_.push_back(shared_ptr<AbstractAction>(response_handler_action));
  actions_.push_back(shared_ptr<AbstractAction>(filesystem_copier_action));
  actions_.push_back(shared_ptr<AbstractAction>(
      kernel_filesystem_copier_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_action));
  actions_.push_back(shared_ptr<AbstractAction>(
      postinstall_runner_action_precommit));
  actions_.push_back(shared_ptr<AbstractAction>(set_bootable_flag_action));
  actions_.push_back(shared_ptr<AbstractAction>(
      postinstall_runner_action_postcommit));
  
  // Enqueue the actions
  for (vector<shared_ptr<AbstractAction> >::iterator it = actions_.begin();
       it != actions_.end(); ++it) {
    processor_.EnqueueAction(it->get());
  }

  // Bond them together. We have to use the leaf-types when calling
  // BondActions().
  BondActions(request_prep_action.get(), update_check_action.get());
  BondActions(update_check_action.get(), response_handler_action.get());
  BondActions(response_handler_action.get(), filesystem_copier_action.get());
  BondActions(response_handler_action.get(),
              kernel_filesystem_copier_action.get());
  BondActions(kernel_filesystem_copier_action.get(),
              download_action.get());
  BondActions(download_action.get(), postinstall_runner_action_precommit.get());
  BondActions(postinstall_runner_action_precommit.get(),
              set_bootable_flag_action.get());
  BondActions(set_bootable_flag_action.get(),
              postinstall_runner_action_postcommit.get());

  processor_.StartProcessing();
}

// Delegate method:
void UpdateAttempter::ProcessingDone(const ActionProcessor* processor,
                                     bool success) {
  CHECK(response_handler_action_);
  if (response_handler_action_->GotNoUpdateResponse()) {
    // All done.
    g_main_loop_quit(loop_);
    return;
  }
  if (!success) {
    if (!full_update_) {
      LOG(ERROR) << "Update failed. Attempting full update";
      actions_.clear();
      response_handler_action_.reset();
      Update(true);
      return;
    } else {
      LOG(ERROR) << "Full update failed. Aborting";
    }
  }
  g_main_loop_quit(loop_);
}

// Stop updating. An attempt will be made to record status to the disk
// so that updates can be resumed later.
void UpdateAttempter::Terminate() {
  // TODO(adlr): implement this method.
  NOTIMPLEMENTED();
}

// Try to resume from a previously Terminate()d update.
void UpdateAttempter::ResumeUpdating() {
  // TODO(adlr): implement this method.
  NOTIMPLEMENTED();
}

bool UpdateAttempter::GetStatus(int64_t* last_checked_time,
                                double* progress,
                                std::string* current_operation,
                                std::string* new_version,
                                int64_t* new_size) {
  // TODO(adlr): Return actual status.
  *last_checked_time = 123;
  *progress = 0.2223;
  *current_operation = "DOWNLOADING";
  *new_version = "0.2.3.8";
  *new_size = 10002;
  return true;
}


}  // namespace chromeos_update_engine
