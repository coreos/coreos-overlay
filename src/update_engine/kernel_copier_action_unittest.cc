// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "update_engine/kernel_copier_action.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

class KernelCopierActionTest : public ::testing::Test { };

TEST(KernelCopierActionTest, SuccessfulCopyTest) {
  const int kDefaultKernelSize = 500;
  string a_file;
  string b_file;

  if (!(utils::MakeTempFile("/tmp/a_file.XXXXXX", &a_file, NULL) &&
        utils::MakeTempFile("/tmp/b_file.XXXXXX", &b_file, NULL))) {
    ADD_FAILURE();
    return;
  }
  ScopedPathUnlinker a_file_unlinker(a_file);
  ScopedPathUnlinker b_file_unlinker(b_file);

  // Make random data for a.
  vector<char> a_data(kDefaultKernelSize);
  FillWithData(&a_data);
  if (!WriteFileVector(a_file, a_data)) {
    ADD_FAILURE();
    return;
  }

  LOG(INFO) << "copying: " << a_file << " -> " << b_file
            << ", "<< kDefaultKernelSize << " bytes";

  // Set up the action objects
  ActionProcessor processor;
  ActionTestDelegate<KernelCopierAction> delegate;

  processor.set_delegate(&delegate);

  ObjectFeederAction<InstallPlan> feeder_action;
  KernelCopierAction copier_action;
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&feeder_action, &copier_action);
  BondActions(&copier_action, &collector_action);

  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&copier_action);
  processor.EnqueueAction(&collector_action);

  InstallPlan install_plan;
  install_plan.kernel_path = b_file;
  install_plan.old_kernel_path = a_file;
  feeder_action.set_obj(install_plan);

  processor.StartProcessing();
  EXPECT_FALSE(processor.IsRunning());
  EXPECT_TRUE(delegate.ran());
  EXPECT_EQ(kActionCodeSuccess, delegate.code());

  vector<char> a_out, b_out;
  EXPECT_TRUE(utils::ReadFile(a_file, &a_out)) << "file failed: " << a_file;
  EXPECT_TRUE(utils::ReadFile(b_file, &b_out)) << "file failed: " << b_file;
  EXPECT_EQ(a_data, a_out);
  EXPECT_EQ(a_data, b_out);

  EXPECT_EQ(collector_action.object(), install_plan);
  EXPECT_FALSE(collector_action.object().old_kernel_hash.empty());

  vector<char> expect_hash;
  EXPECT_TRUE(OmahaHashCalculator::RawHashOfData(a_data, &expect_hash));
  EXPECT_EQ(expect_hash, collector_action.object().old_kernel_hash);
}

TEST(KernelCopierActionTest, MissingInputObjectTest) {
  ActionProcessor processor;
  ActionTestDelegate<KernelCopierAction> delegate;

  processor.set_delegate(&delegate);

  KernelCopierAction copier_action;
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&copier_action, &collector_action);

  processor.EnqueueAction(&copier_action);
  processor.EnqueueAction(&collector_action);
  processor.StartProcessing();
  EXPECT_FALSE(processor.IsRunning());
  EXPECT_TRUE(delegate.ran());
  EXPECT_EQ(kActionCodeError, delegate.code());
}

TEST(KernelCopierActionTest, ResumeTest) {
  ActionProcessor processor;
  ActionTestDelegate<KernelCopierAction> delegate;

  processor.set_delegate(&delegate);

  ObjectFeederAction<InstallPlan> feeder_action;
  const char* kUrl = "http://some/url";
  InstallPlan install_plan(true, kUrl, 0, "", "");
  feeder_action.set_obj(install_plan);
  KernelCopierAction copier_action;
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&feeder_action, &copier_action);
  BondActions(&copier_action, &collector_action);

  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&copier_action);
  processor.EnqueueAction(&collector_action);
  processor.StartProcessing();
  EXPECT_FALSE(processor.IsRunning());
  EXPECT_TRUE(delegate.ran());
  EXPECT_EQ(kActionCodeSuccess, delegate.code());
  EXPECT_EQ(kUrl, collector_action.object().download_url);
}

TEST(KernelCopierActionTest, MissingSourceFile) {
  ActionProcessor processor;
  ActionTestDelegate<KernelCopierAction> delegate;

  processor.set_delegate(&delegate);

  ObjectFeederAction<InstallPlan> feeder_action;
  InstallPlan install_plan;
  install_plan.kernel_path = "/no/such/file";
  install_plan.old_kernel_path = "/also/no/such/file";
  feeder_action.set_obj(install_plan);
  KernelCopierAction copier_action;
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&copier_action, &collector_action);

  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&copier_action);
  processor.EnqueueAction(&collector_action);
  processor.StartProcessing();
  EXPECT_FALSE(processor.IsRunning());
  EXPECT_TRUE(delegate.ran());
  EXPECT_EQ(kActionCodeError, delegate.code());
}

}  // namespace chromeos_update_engine
