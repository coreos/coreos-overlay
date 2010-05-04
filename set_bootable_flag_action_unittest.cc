// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "update_engine/set_bootable_flag_action.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

class SetBootableFlagActionTest : public ::testing::Test {};

// These disabled tests are a reminder this needs to change.
TEST_F(SetBootableFlagActionTest, DISABLED_SimpleTest) {
  // TODO(adlr): find a way to test this object.
}

TEST_F(SetBootableFlagActionTest, DISABLED_BadDeviceTest) {
}

TEST_F(SetBootableFlagActionTest, DISABLED_NoInputObjectTest) {
}

}  // namespace chromeos_update_engine
