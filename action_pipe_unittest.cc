// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include "update_engine/action.h"
#include "update_engine/action_pipe.h"

namespace chromeos_update_engine {

using chromeos_update_engine::ActionPipe;

class ActionPipeTestAction;

template<>
class ActionTraits<ActionPipeTestAction> {
 public:
  typedef string OutputObjectType;
  typedef string InputObjectType;
};

// This is a simple Action class for testing.
struct ActionPipeTestAction : public Action<ActionPipeTestAction> {
  typedef string InputObjectType;
  typedef string OutputObjectType;
  ActionPipe<string>* in_pipe() { return in_pipe_.get(); }
  ActionPipe<string>* out_pipe() { return out_pipe_.get(); }
  void PerformAction() {}
  string Type() const { return "ActionPipeTestAction"; }
};

class ActionPipeTest : public ::testing::Test { };

// This test creates two simple Actions and sends a message via an ActionPipe
// from one to the other.
TEST(ActionPipeTest, SimpleTest) {
  ActionPipeTestAction a, b;
  BondActions(&a, &b);
  a.out_pipe()->set_contents("foo");
  EXPECT_EQ("foo", b.in_pipe()->contents());
}

}  // namespace chromeos_update_engine
