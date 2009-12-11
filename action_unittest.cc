// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <gtest/gtest.h>
#include "update_engine/action.h"
#include "update_engine/action_processor.h"

using std::string;

namespace chromeos_update_engine {

using chromeos_update_engine::ActionPipe;

class ActionTestAction;

template<>
class ActionTraits<ActionTestAction> {
 public:
  typedef string OutputObjectType;
  typedef string InputObjectType;
};

// This is a simple Action class for testing.
struct ActionTestAction : public Action<ActionTestAction> {
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
  string Type() const { return "ActionTestAction"; }
};

class ActionTest : public ::testing::Test { };

// This test creates two simple Actions and sends a message via an ActionPipe
// from one to the other.
TEST(ActionTest, SimpleTest) {
  ActionTestAction action;

  EXPECT_FALSE(action.in_pipe());
  EXPECT_FALSE(action.out_pipe());
  EXPECT_FALSE(action.processor());
  EXPECT_FALSE(action.IsRunning());

  ActionProcessor action_processor;
  action_processor.EnqueueAction(&action);
  EXPECT_EQ(&action_processor, action.processor());

  action_processor.StartProcessing();
  EXPECT_TRUE(action.IsRunning());
  action.CompleteAction();
  EXPECT_FALSE(action.IsRunning());
}

}  // namespace chromeos_update_engine
