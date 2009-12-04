// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include "update_engine/action.h"
#include "update_engine/action_processor.h"

namespace chromeos_update_engine {

using chromeos_update_engine::ActionPipe;

class ActionProcessorTestAction;

template<>
class ActionTraits<ActionProcessorTestAction> {
 public:
  typedef string OutputObjectType;
  typedef string InputObjectType;
};

// This is a simple Action class for testing.
struct ActionProcessorTestAction : public Action<ActionProcessorTestAction> {
  typedef string InputObjectType;
  typedef string OutputObjectType;
  ActionPipe<string>* in_pipe() { return in_pipe_.get(); }
  ActionPipe<string>* out_pipe() { return out_pipe_.get(); }
  ActionProcessor* processor() { return processor_; }
  void PerformAction() {}
  void CompleteAction() {
    ASSERT_TRUE(processor());
    processor()->ActionComplete(this, true);
  }
  string Type() const { return "ActionProcessorTestAction"; }
};

class ActionProcessorTest : public ::testing::Test { };

// This test creates two simple Actions and sends a message via an ActionPipe
// from one to the other.
TEST(ActionProcessorTest, SimpleTest) {
  ActionProcessorTestAction action;
  ActionProcessor action_processor;
  EXPECT_FALSE(action_processor.IsRunning());
  action_processor.EnqueueAction(&action);
  EXPECT_FALSE(action_processor.IsRunning());
  EXPECT_FALSE(action.IsRunning());
  action_processor.StartProcessing();
  EXPECT_TRUE(action_processor.IsRunning());
  EXPECT_TRUE(action.IsRunning());
  EXPECT_EQ(action_processor.current_action(), &action);
  action.CompleteAction();
  EXPECT_FALSE(action_processor.IsRunning());
  EXPECT_FALSE(action.IsRunning());
}

namespace {
class MyActionProcessorDelegate : public ActionProcessorDelegate {
 public:
  explicit MyActionProcessorDelegate(const ActionProcessor* processor)
      : processor_(processor), processing_done_called_(false) {}

  virtual void ProcessingDone(const ActionProcessor* processor) {
    EXPECT_EQ(processor_, processor);
    EXPECT_FALSE(processing_done_called_);
    processing_done_called_ = true;
  }
  virtual void ProcessingStopped(const ActionProcessor* processor) {
    EXPECT_EQ(processor_, processor);
    EXPECT_FALSE(processing_stopped_called_);
    processing_stopped_called_ = true;
  }
  virtual void ActionCompleted(ActionProcessor* processor,
                               AbstractAction* action,
                               bool success) {
    EXPECT_EQ(processor_, processor);
    EXPECT_FALSE(action_completed_called_);
    action_completed_called_ = true;
    action_completed_success_ = success;
  }

  const ActionProcessor* processor_;
  bool processing_done_called_;
  bool processing_stopped_called_;
  bool action_completed_called_;
  bool action_completed_success_;
};
}  // namespace {}

TEST(ActionProcessorTest, DelegateTest) {
  ActionProcessorTestAction action;
  ActionProcessor action_processor;
  MyActionProcessorDelegate delegate(&action_processor);
  action_processor.set_delegate(&delegate);

  action_processor.EnqueueAction(&action);
  action_processor.StartProcessing();
  action.CompleteAction();
  action_processor.set_delegate(NULL);
  EXPECT_TRUE(delegate.processing_done_called_);
  EXPECT_TRUE(delegate.action_completed_called_);
}

TEST(ActionProcessorTest, StopProcessingTest) {
  ActionProcessorTestAction action;
  ActionProcessor action_processor;
  MyActionProcessorDelegate delegate(&action_processor);
  action_processor.set_delegate(&delegate);

  action_processor.EnqueueAction(&action);
  action_processor.StartProcessing();
  action_processor.StopProcessing();
  action_processor.set_delegate(NULL);
  EXPECT_TRUE(delegate.processing_stopped_called_);
  EXPECT_FALSE(delegate.action_completed_called_);
  EXPECT_FALSE(action_processor.IsRunning());
  EXPECT_EQ(NULL, action_processor.current_action());
}

TEST(ActionProcessorTest, ChainActionsTest) {
  ActionProcessorTestAction action1, action2;
  ActionProcessor action_processor;
  action_processor.EnqueueAction(&action1);
  action_processor.EnqueueAction(&action2);
  action_processor.StartProcessing();
  EXPECT_EQ(&action1, action_processor.current_action());
  EXPECT_TRUE(action_processor.IsRunning());
  action1.CompleteAction();
  EXPECT_EQ(&action2, action_processor.current_action());
  EXPECT_TRUE(action_processor.IsRunning());
  action2.CompleteAction();
  EXPECT_EQ(NULL, action_processor.current_action());
  EXPECT_FALSE(action_processor.IsRunning());
}

TEST(ActionProcessorTest, DtorTest) {
  ActionProcessorTestAction action1, action2;
  {
    ActionProcessor action_processor;
    action_processor.EnqueueAction(&action1);
    action_processor.EnqueueAction(&action2);
    action_processor.StartProcessing();
  }
  EXPECT_EQ(NULL, action1.processor());
  EXPECT_FALSE(action1.IsRunning());
  EXPECT_EQ(NULL, action2.processor());
  EXPECT_FALSE(action2.IsRunning());
}

TEST(ActionProcessorTest, DefaultDelegateTest) {
  // Just make sure it doesn't crash
  ActionProcessorTestAction action;
  ActionProcessor action_processor;
  ActionProcessorDelegate delegate;
  action_processor.set_delegate(&delegate);

  action_processor.EnqueueAction(&action);
  action_processor.StartProcessing();
  action.CompleteAction();

  action_processor.EnqueueAction(&action);
  action_processor.StartProcessing();
  action_processor.StopProcessing();

  action_processor.set_delegate(NULL);
}

}  // namespace chromeos_update_engine
