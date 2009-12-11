// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tr1/memory>
#include <vector>
#include <gflags/gflags.h>
#include <glib.h>
#include "chromeos/obsolete_logging.h"
#include "update_engine/action_processor.h"
#include "update_engine/download_action.h"
#include "update_engine/filesystem_copier_action.h"
#include "update_engine/install_action.h"
#include "update_engine/libcurl_http_fetcher.h"
#include "update_engine/omaha_request_prep_action.h"
#include "update_engine/omaha_response_handler_action.h"
#include "update_engine/postinstall_runner_action.h"
#include "update_engine/set_bootable_flag_action.h"
#include "update_engine/update_check_action.h"

using std::string;
using std::tr1::shared_ptr;
using std::vector;

namespace chromeos_update_engine {

class UpdateAttempter : public ActionProcessorDelegate {
 public:
  UpdateAttempter(GMainLoop *loop)
      : full_update_(false),
        loop_(loop) {}
  void Update(bool force_full_update);
  
  // Delegate method:
  void ProcessingDone(const ActionProcessor* processor, bool success);
 private:
  bool full_update_;
  vector<shared_ptr<AbstractAction> > actions_;
  ActionProcessor processor_;
  GMainLoop *loop_;

  // pointer to the OmahaResponseHandlerAction in the actions_ vector;
  shared_ptr<OmahaResponseHandlerAction> response_handler_action_;
  DISALLOW_COPY_AND_ASSIGN(UpdateAttempter);
};

// Returns true on success. If there was no update available, that's still
// success.
// If force_full is true, try to force a full update.
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
      new FilesystemCopierAction);
  shared_ptr<DownloadAction> download_action(
      new DownloadAction(new LibcurlHttpFetcher));
  shared_ptr<InstallAction> install_action(
      new InstallAction);
  shared_ptr<PostinstallRunnerAction> postinstall_runner_action(
      new PostinstallRunnerAction);
  shared_ptr<SetBootableFlagAction> set_bootable_flag_action(
      new SetBootableFlagAction);
      
  response_handler_action_ = response_handler_action;

  actions_.push_back(shared_ptr<AbstractAction>(request_prep_action));
  actions_.push_back(shared_ptr<AbstractAction>(update_check_action));
  actions_.push_back(shared_ptr<AbstractAction>(response_handler_action));
  actions_.push_back(shared_ptr<AbstractAction>(filesystem_copier_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_action));
  actions_.push_back(shared_ptr<AbstractAction>(install_action));
  actions_.push_back(shared_ptr<AbstractAction>(postinstall_runner_action));
  actions_.push_back(shared_ptr<AbstractAction>(set_bootable_flag_action));
  
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
  BondActions(filesystem_copier_action.get(), download_action.get());
  BondActions(download_action.get(), install_action.get());
  BondActions(install_action.get(), postinstall_runner_action.get());
  BondActions(postinstall_runner_action.get(), set_bootable_flag_action.get());

  processor_.StartProcessing();
}

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

gboolean UpdateInMainLoop(void* arg) {
  UpdateAttempter* update_attempter = reinterpret_cast<UpdateAttempter*>(arg);
  update_attempter->Update(false);
  return FALSE;  // Don't call this callback function again
}

}  // namespace chromeos_update_engine

#include "update_engine/subprocess.h"

int main(int argc, char** argv) {
  g_thread_init(NULL);
  chromeos_update_engine::Subprocess::Init();
  google::ParseCommandLineFlags(&argc, &argv, true);
  // TODO(adlr): figure out log file
  logging::InitLogging("",
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
  LOG(INFO) << "Chrome OS Update Engine starting";
  
  // Create the single GMainLoop
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);

  chromeos_update_engine::UpdateAttempter update_attempter(loop);

  g_timeout_add(0, &chromeos_update_engine::UpdateInMainLoop,
                &update_attempter);

  g_main_loop_run(loop);
  g_main_loop_unref(loop);

  LOG(INFO) << "Chrome OS Update Engine terminating";
  return 0;
}
