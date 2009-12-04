// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>
#include <gtest/gtest.h>
#include "update_engine/install_plan.h"
#include "update_engine/omaha_request_prep_action.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

class OmahaRequestPrepActionTest : public ::testing::Test {
 public:
  // Return true iff the OmahaResponseHandlerAction succeeded.
  // if out is non-NULL, it's set w/ the response from the action.
  bool DoTest(bool force_full_update, UpdateCheckParams* out);
  static const string kTestDir;
};

const string OmahaRequestPrepActionTest::kTestDir = "request_prep_action-test";

class OmahaRequestPrepActionProcessorDelegate
    : public ActionProcessorDelegate {
 public:
  OmahaRequestPrepActionProcessorDelegate()
      : success_(false),
        success_set_(false) {}
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       bool success) {
    if (action->Type() == OmahaRequestPrepAction::StaticType()) {
      success_ = success;
      success_set_ = true;
    }
  }
  bool success_;
  bool success_set_;
};

bool OmahaRequestPrepActionTest::DoTest(bool force_full_update,
                                        UpdateCheckParams* out) {
  ActionProcessor processor;
  OmahaRequestPrepActionProcessorDelegate delegate;
  processor.set_delegate(&delegate);

  OmahaRequestPrepAction request_prep_action(force_full_update);
  request_prep_action.set_root(string("./") + kTestDir);
  ObjectCollectorAction<UpdateCheckParams> collector_action;
  BondActions(&request_prep_action, &collector_action);
  processor.EnqueueAction(&request_prep_action);
  processor.EnqueueAction(&collector_action);
  processor.StartProcessing();
  EXPECT_TRUE(!processor.IsRunning())
      << "Update test to handle non-asynch actions";
  if (out)
    *out = collector_action.object();
  EXPECT_TRUE(delegate.success_set_);
  return delegate.success_;
}

namespace {
// Returns true iff str is formatted as a mac address
bool IsValidMac(const string& str) {
  if (str.size() != (3 * 6 - 1))
    return false;
  for (unsigned int i = 0; i < str.size(); i++) {
    char c = str[i];
    switch (i % 3) {
      case 0:  // fall through
      case 1:
        if ((c >= '0') && (c <= '9'))
          break;
        if ((c >= 'a') && (c <= 'f'))
          break;
        if ((c >= 'A') && (c <= 'F'))
          break;
        return false;
      case 2:
        if (c == ':')
          break;
        return false;
    }
  }
  return true;
}
string GetMachineType() {
  FILE* fp = popen("uname -m", "r");
  if (!fp)
    return "";
  string ret;
  for (;;) {
    char buffer[10];
    size_t r = fread(buffer, 1, sizeof(buffer), fp);
    if (r == 0)
      break;
    ret.insert(ret.begin(), buffer, buffer + r);
  }
  // strip trailing '\n' if it exists
  if ((*ret.rbegin()) == '\n')
    ret.resize(ret.size() - 1);
  fclose(fp);
  return ret;
}
}  // namespace {}

TEST_F(OmahaRequestPrepActionTest, SimpleTest) {
  ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir + "/etc"));
  {
    ASSERT_TRUE(WriteFileString(
        kTestDir + "/etc/lsb-release",
        "GOOGLE_FOO=bar\nGOOGLE_RELEASE=0.2.2.3\nGOOGLE_TRACK=footrack"));
    UpdateCheckParams out;
    EXPECT_TRUE(DoTest(false, &out));
    EXPECT_TRUE(IsValidMac(out.machine_id));
    // for now we're just using the machine id here
    EXPECT_TRUE(IsValidMac(out.user_id));
    EXPECT_EQ("Chrome OS", out.os_platform);
    EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp);
    EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id);
    EXPECT_EQ("0.2.2.3", out.app_version);
    EXPECT_EQ("en-US", out.app_lang);
    EXPECT_EQ("footrack", out.app_track);
  }
  EXPECT_EQ(0, System(string("rm -rf ") + kTestDir));
}

TEST_F(OmahaRequestPrepActionTest, MissingTrackTest) {
  ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir + "/etc"));
  {
    ASSERT_TRUE(WriteFileString(
        kTestDir + "/etc/lsb-release",
        "GOOGLE_FOO=bar\nGOOGLE_RELEASE=0.2.2.3\nGOOGLE_TRXCK=footrack"));
    UpdateCheckParams out;
    EXPECT_TRUE(DoTest(false, &out));
    EXPECT_TRUE(IsValidMac(out.machine_id));
    // for now we're just using the machine id here
    EXPECT_TRUE(IsValidMac(out.user_id));
    EXPECT_EQ("Chrome OS", out.os_platform);
    EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp);
    EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id);
    EXPECT_EQ("0.2.2.3", out.app_version);
    EXPECT_EQ("en-US", out.app_lang);
    EXPECT_EQ("", out.app_track);
  }
  EXPECT_EQ(0, System(string("rm -rf ") + kTestDir));
}

TEST_F(OmahaRequestPrepActionTest, ConfusingReleaseTest) {
  ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir + "/etc"));
  {
    ASSERT_TRUE(WriteFileString(
        kTestDir + "/etc/lsb-release",
        "GOOGLE_FOO=GOOGLE_RELEASE=1.2.3.4\n"
        "GOOGLE_RELEASE=0.2.2.3\nGOOGLE_TRXCK=footrack"));
    UpdateCheckParams out;
    EXPECT_TRUE(DoTest(false, &out));
    EXPECT_TRUE(IsValidMac(out.machine_id));
    // for now we're just using the machine id here
    EXPECT_TRUE(IsValidMac(out.user_id));
    EXPECT_EQ("Chrome OS", out.os_platform);
    EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp);
    EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id);
    EXPECT_EQ("0.2.2.3", out.app_version);
    EXPECT_EQ("en-US", out.app_lang);
    EXPECT_EQ("", out.app_track);
  }
  EXPECT_EQ(0, System(string("rm -rf ") + kTestDir));
}

}  // namespace chromeos_update_engine
