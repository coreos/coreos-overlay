// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <gtest/gtest.h>
#include "update_engine/omaha_response_handler_action.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

class OmahaResponseHandlerActionTest : public ::testing::Test {
 public:
  // Return true iff the OmahaResponseHandlerAction succeeded.
  // If out is non-NULL, it's set w/ the response from the action.
  bool DoTest(const UpdateCheckResponse& in,
              const string& boot_dev,
              InstallPlan* out);
};

class OmahaResponseHandlerActionProcessorDelegate
    : public ActionProcessorDelegate {
 public:
  OmahaResponseHandlerActionProcessorDelegate()
      : success_(false),
        success_set_(false) {}
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       bool success) {
    if (action->Type() == OmahaResponseHandlerAction::StaticType()) {
      success_ = success;
      success_set_ = true;
    }
  }
  bool success_;
  bool success_set_;
};

namespace {
const string kLongName =
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "very_long_name_and_no_slashes-very_long_name_and_no_slashes"
    "-the_update_a.b.c.d_DELTA_.tgz";
}  // namespace {}

bool OmahaResponseHandlerActionTest::DoTest(const UpdateCheckResponse& in,
                                            const string& boot_dev,
                                            InstallPlan* out) {
  ActionProcessor processor;
  OmahaResponseHandlerActionProcessorDelegate delegate;
  processor.set_delegate(&delegate);

  ObjectFeederAction<UpdateCheckResponse> feeder_action;
  feeder_action.set_obj(in);
  OmahaResponseHandlerAction response_handler_action;
  response_handler_action.set_boot_device(boot_dev);
  BondActions(&feeder_action, &response_handler_action);
  ObjectCollectorAction<InstallPlan> collector_action;
  BondActions(&response_handler_action, &collector_action);
  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&response_handler_action);
  processor.EnqueueAction(&collector_action);
  processor.StartProcessing();
  EXPECT_TRUE(!processor.IsRunning())
      << "Update test to handle non-asynch actions";
  if (out)
    *out = collector_action.object();
  EXPECT_TRUE(delegate.success_set_);
  return delegate.success_;
}

TEST_F(OmahaResponseHandlerActionTest, SimpleTest) {
  {
    UpdateCheckResponse in;
    in.update_exists = true;
    in.display_version = "a.b.c.d";
    in.codebase = "http://foo/the_update_a.b.c.d_FULL_.tgz";
    in.more_info_url = "http://more/info";
    in.hash = "HASH+";
    in.size = 12;
    in.needs_admin = true;
    in.prompt = false;
    InstallPlan install_plan;
    EXPECT_TRUE(DoTest(in, "/dev/sda1", &install_plan));
    EXPECT_TRUE(install_plan.is_full_update);
    EXPECT_EQ(in.codebase, install_plan.download_url);
    EXPECT_EQ(in.hash, install_plan.download_hash);
    EXPECT_EQ(string(utils::kStatefulPartition) +
              "/the_update_a.b.c.d_FULL_.tgz",
              install_plan.download_path);
    EXPECT_EQ("/dev/sda2", install_plan.install_path);
  }
  {
    UpdateCheckResponse in;
    in.update_exists = true;
    in.display_version = "a.b.c.d";
    in.codebase = "http://foo/the_update_a.b.c.d_DELTA_.tgz";
    in.more_info_url = "http://more/info";
    in.hash = "HASHj+";
    in.size = 12;
    in.needs_admin = true;
    in.prompt = true;
    InstallPlan install_plan;
    EXPECT_TRUE(DoTest(in, "/dev/sda4", &install_plan));
    EXPECT_FALSE(install_plan.is_full_update);
    EXPECT_EQ(in.codebase, install_plan.download_url);
    EXPECT_EQ(in.hash, install_plan.download_hash);
    EXPECT_EQ(string(utils::kStatefulPartition) +
              "/the_update_a.b.c.d_DELTA_.tgz",
              install_plan.download_path);
    EXPECT_EQ("/dev/sda3", install_plan.install_path);
  }
  {
    UpdateCheckResponse in;
    in.update_exists = true;
    in.display_version = "a.b.c.d";
    in.codebase = kLongName;
    in.more_info_url = "http://more/info";
    in.hash = "HASHj+";
    in.size = 12;
    in.needs_admin = true;
    in.prompt = true;
    InstallPlan install_plan;
    EXPECT_TRUE(DoTest(in, "/dev/sda4", &install_plan));
    EXPECT_FALSE(install_plan.is_full_update);
    EXPECT_EQ(in.codebase, install_plan.download_url);
    EXPECT_EQ(in.hash, install_plan.download_hash);
    EXPECT_EQ(string(utils::kStatefulPartition) + "/" +
              kLongName.substr(0, 255),
              install_plan.download_path);
    EXPECT_EQ("/dev/sda3", install_plan.install_path);
  }
}

TEST_F(OmahaResponseHandlerActionTest, NoUpdatesTest) {
  UpdateCheckResponse in;
  in.update_exists = false;
  InstallPlan install_plan;
  EXPECT_FALSE(DoTest(in, "/dev/sda1", &install_plan));
  EXPECT_FALSE(install_plan.is_full_update);
  EXPECT_EQ("", install_plan.download_url);
  EXPECT_EQ("", install_plan.download_hash);
  EXPECT_EQ("", install_plan.download_path);
  EXPECT_EQ("", install_plan.install_path);
}

}  // namespace chromeos_update_engine
