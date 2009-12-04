// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/action_processor.h"
#include "chromeos/obsolete_logging.h"
#include "update_engine/action.h"

namespace chromeos_update_engine {

ActionProcessor::ActionProcessor()
    : current_action_(NULL), delegate_(NULL) {}

ActionProcessor::~ActionProcessor() {
  if (IsRunning()) {
    StopProcessing();
  }
  for (std::deque<AbstractAction*>::iterator it = actions_.begin();
       it != actions_.end(); ++it) {
    (*it)->SetProcessor(NULL);
  }
}

void ActionProcessor::EnqueueAction(AbstractAction* action) {
  actions_.push_back(action);
  action->SetProcessor(this);
}

void ActionProcessor::StartProcessing() {
  CHECK(!IsRunning());
  if (!actions_.empty()) {
    current_action_ = actions_.front();
    LOG(INFO) << "ActionProcessor::StartProcessing: "
              << current_action_->Type();
    actions_.pop_front();
    current_action_->PerformAction();
  }
}

void ActionProcessor::StopProcessing() {
  CHECK(IsRunning());
  CHECK(current_action_);
  current_action_->TerminateProcessing();
  CHECK(current_action_);
  current_action_->SetProcessor(NULL);
  LOG(INFO) << "ActionProcessor::StopProcessing: aborted "
            << current_action_->Type();
  current_action_ = NULL;
  if (delegate_)
    delegate_->ProcessingStopped(this);
}

void ActionProcessor::ActionComplete(AbstractAction* actionptr,
                                     bool success) {
  CHECK_EQ(actionptr, current_action_);
  if (delegate_)
    delegate_->ActionCompleted(this, actionptr, success);
  string old_type = current_action_->Type();
  current_action_->SetProcessor(NULL);
  current_action_ = NULL;
  if (actions_.empty()) {
    LOG(INFO) << "ActionProcessor::ActionComplete: finished last action of"
                 " type " << old_type;
  } else if (!success) {
    LOG(INFO) << "ActionProcessor::ActionComplete: " << old_type
              << " action failed. Aborting processing.";
    actions_.clear();
  }
  if (actions_.empty()) {
    LOG(INFO) << "ActionProcessor::ActionComplete: finished last action of"
                 " type " << old_type;
    if (delegate_) {
      delegate_->ProcessingDone(this);
    }
    return;
  }
  current_action_ = actions_.front();
  actions_.pop_front();
  LOG(INFO) << "ActionProcessor::ActionComplete: finished " << old_type
            << ", starting " << current_action_->Type();
  current_action_->PerformAction();
}

}  // namespace chromeos_update_engine
