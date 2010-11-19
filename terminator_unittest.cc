// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

#include "update_engine/terminator.h"

using std::string;
using testing::ExitedWithCode;

namespace chromeos_update_engine {

class TerminatorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    Terminator::Init();
    ASSERT_FALSE(Terminator::exit_blocked());
    ASSERT_FALSE(Terminator::exit_requested());
  }
  virtual void TearDown() {
    // Makes sure subsequent non-Terminator tests don't get accidentally
    // terminated.
    Terminator::Init();
  }
};

typedef TerminatorTest TerminatorDeathTest;

namespace {
void UnblockExitThroughUnblocker() {
  ScopedTerminatorExitUnblocker unblocker = ScopedTerminatorExitUnblocker();
}

void RaiseSIGTERM() {
  ASSERT_EXIT(raise(SIGTERM), ExitedWithCode(0), "");
}
}  // namespace {}

TEST_F(TerminatorTest, HandleSignalTest) {
  Terminator::set_exit_blocked(true);
  Terminator::HandleSignal(SIGTERM);
  ASSERT_TRUE(Terminator::exit_requested());
}

TEST_F(TerminatorTest, ScopedTerminatorExitUnblockerTest) {
  Terminator::set_exit_blocked(true);
  ASSERT_TRUE(Terminator::exit_blocked());
  ASSERT_FALSE(Terminator::exit_requested());
  UnblockExitThroughUnblocker();
  ASSERT_FALSE(Terminator::exit_blocked());
  ASSERT_FALSE(Terminator::exit_requested());
}

TEST_F(TerminatorDeathTest, ExitTest) {
  ASSERT_EXIT(Terminator::Exit(), ExitedWithCode(0), "");
  Terminator::set_exit_blocked(true);
  ASSERT_EXIT(Terminator::Exit(), ExitedWithCode(0), "");
}

TEST_F(TerminatorDeathTest, RaiseSignalTest) {
  RaiseSIGTERM();
  Terminator::set_exit_blocked(true);
  EXPECT_FATAL_FAILURE(RaiseSIGTERM(), "");
}

TEST_F(TerminatorDeathTest, ScopedTerminatorExitUnblockerExitTest) {
  Terminator::set_exit_blocked(true);
  Terminator::exit_requested_ = 1;
  ASSERT_EXIT(UnblockExitThroughUnblocker(), ExitedWithCode(0), "");
}

}  // namespace chromeos_update_engine
