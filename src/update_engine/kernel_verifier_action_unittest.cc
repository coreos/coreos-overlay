// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "update_engine/kernel_verifier_action.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
const int kDefaultKernelSize = 500; // Something small for a test

enum VerifyTest {
  VerifySuccess,
  VerifyBadSize,
  VerifyBadHash,
};

void DoTest(VerifyTest test) {
  string kernel_file;

  if (!utils::MakeTempFile("/tmp/kernel_file.XXXXXX", &kernel_file, NULL)) {
    ADD_FAILURE();
    return;
  }
  ScopedPathUnlinker kernel_file_unlinker(kernel_file);

  // Write some random data to our "kernel"
  vector<char> kernel_data(500);
  FillWithData(&kernel_data);
  if (!WriteFileVector(kernel_file, kernel_data)) {
    ADD_FAILURE();
    return;
  }

  if (test == VerifyBadSize) {
    kernel_data.push_back(0xff);
  }

  vector<char> kernel_hash;
  if (test == VerifyBadHash) {
    kernel_hash.assign(32, 0xff);
  } else {
    if (!OmahaHashCalculator::RawHashOfData(kernel_data, &kernel_hash)) {
      ADD_FAILURE();
      return;
    }
  }

  ActionProcessor processor;
  ActionTestDelegate<KernelVerifierAction> delegate;

  ObjectFeederAction<InstallPlan> feeder_action;
  KernelVerifierAction verifier_action;
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&feeder_action, &verifier_action);
  BondActions(&verifier_action, &collector_action);

  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&verifier_action);
  processor.EnqueueAction(&collector_action);

  InstallPlan install_plan;
  install_plan.kernel_path = kernel_file;
  install_plan.new_kernel_size = kernel_data.size();
  install_plan.new_kernel_hash = kernel_hash;
  feeder_action.set_obj(install_plan);

  delegate.RunProcessor(&processor);
  EXPECT_TRUE(delegate.ran());

  if (test == VerifySuccess) {
    EXPECT_EQ(kActionCodeSuccess, delegate.code());
    EXPECT_EQ(collector_action.object(), install_plan);
  } else {
    EXPECT_EQ(kActionCodeNewKernelVerificationError, delegate.code());
  }
}
}  // namespace

class KernelVerifierActionTest : public ::testing::Test { };

TEST(KernelVerifierActionTest, VerifySuccessTest) {
  DoTest(VerifySuccess);
}

TEST(KernelVerifierActionTest, VerifyBadSizeTest) {
  DoTest(VerifyBadSize);
}

TEST(KernelVerifierActionTest, VerifyBadHashTest) {
  DoTest(VerifyBadHash);
}

TEST(KernelVerifierActionTest, MissingInputObjectTest) {
  ActionProcessor processor;
  ActionTestDelegate<KernelVerifierAction> delegate;

  KernelVerifierAction verifier_action;
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&verifier_action, &collector_action);

  processor.EnqueueAction(&verifier_action);
  processor.EnqueueAction(&collector_action);
  delegate.RunProcessor(&processor);
  EXPECT_TRUE(delegate.ran());
  EXPECT_EQ(kActionCodeNewKernelVerificationError, delegate.code());
}

TEST(KernelVerifierActionTest, MissingKernelTest) {
  ActionProcessor processor;
  ActionTestDelegate<KernelVerifierAction> delegate;

  ObjectFeederAction<InstallPlan> feeder_action;
  InstallPlan install_plan;
  install_plan.kernel_path = "/no/such/file";
  feeder_action.set_obj(install_plan);
  KernelVerifierAction verifier_action;
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&verifier_action, &collector_action);

  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&verifier_action);
  processor.EnqueueAction(&collector_action);
  delegate.RunProcessor(&processor);
  EXPECT_TRUE(delegate.ran());
  EXPECT_EQ(kActionCodeNewKernelVerificationError, delegate.code());
}

}  // namespace chromeos_update_engine
