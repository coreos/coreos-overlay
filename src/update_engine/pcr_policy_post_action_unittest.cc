// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "update_engine/mock_http_fetcher.h"
#include "update_engine/mock_system_state.h"
#include "update_engine/pcr_policy_post_action.h"
#include "update_engine/omaha_hash_calculator.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
const string kServerResponse = "SUCCESS\n";

enum VerifyTest {
  VerifySuccess,
  VerifyBadSize,
  VerifyBadHash,
  HTTPServerDisabled,
  HTTPServerError,
};

void DoTest(VerifyTest test) {
  string pcrs_file;

  if (!utils::MakeTempFile("/tmp/pcrs_file.XXXXXX", &pcrs_file, NULL)) {
    ADD_FAILURE();
    return;
  }
  ScopedPathUnlinker pcrs_file_unlinker(pcrs_file);

  vector<char> pcrs_data(500);
  FillWithData(&pcrs_data);
  if (!WriteFileVector(pcrs_file, pcrs_data)) {
    ADD_FAILURE();
    return;
  }

  if (test == VerifyBadSize) {
    pcrs_data.push_back(0xff);
  }

  vector<char> pcrs_hash;
  if (test == VerifyBadHash) {
    pcrs_hash.assign(32, 0xff);
  } else {
    if (!OmahaHashCalculator::RawHashOfData(pcrs_data, &pcrs_hash)) {
      ADD_FAILURE();
      return;
    }
  }

  MockSystemState mock_system_state;
  OmahaRequestParams params(&mock_system_state);
  mock_system_state.set_request_params(&params);
  MockHttpFetcher *fetcher = new MockHttpFetcher(kServerResponse.data(),
                                                 kServerResponse.size());
  if (test == HTTPServerError)
    fetcher->FailTransfer(kHttpResponseInternalServerError);
  if (test == HTTPServerDisabled)
    fetcher->set_never_use(true);
  else
    params.set_pcr_policy_url("http://somewhere");

  ActionProcessor processor;
  ActionTestDelegate<PCRPolicyPostAction> delegate;

  ObjectFeederAction<InstallPlan> feeder_action;
  PCRPolicyPostAction post_action(&mock_system_state, fetcher);
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&feeder_action, &post_action);
  BondActions(&post_action, &collector_action);

  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&post_action);
  processor.EnqueueAction(&collector_action);

  InstallPlan install_plan;
  install_plan.pcr_policy_path = pcrs_file;
  install_plan.new_pcr_policy_size = pcrs_data.size();
  install_plan.new_pcr_policy_hash = pcrs_hash;
  feeder_action.set_obj(install_plan);

  delegate.RunProcessorInMainLoop(&processor);
  EXPECT_TRUE(delegate.ran());

  if (test == VerifySuccess || test == HTTPServerDisabled) {
    EXPECT_EQ(kActionCodeSuccess, delegate.code());
    EXPECT_EQ(collector_action.object(), install_plan);
  } else if (test == HTTPServerError) {
    EXPECT_EQ(kActionCodeNewPCRPolicyHTTPError, delegate.code());
  } else {
    EXPECT_EQ(kActionCodeNewPCRPolicyVerificationError, delegate.code());
  }
}
}  // namespace

class PCRPolicyPostActionTest : public ::testing::Test { };

TEST(PCRPolicyPostActionTest, VerifySuccessTest) {
  DoTest(VerifySuccess);
}

TEST(PCRPolicyPostActionTest, VerifyBadSizeTest) {
  DoTest(VerifyBadSize);
}

TEST(PCRPolicyPostActionTest, VerifyBadHashTest) {
  DoTest(VerifyBadHash);
}

TEST(PCRPolicyPostActionTest, HTTPServerDisabledTest) {
  DoTest(HTTPServerDisabled);
}

TEST(PCRPolicyPostActionTest, HTTPServerErrorTest) {
  DoTest(HTTPServerError);
}

TEST(PCRPolicyPostActionTest, MissingInputObjectTest) {
  ActionProcessor processor;
  ActionTestDelegate<PCRPolicyPostAction> delegate;

  PCRPolicyPostAction post_action(nullptr, nullptr);
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&post_action, &collector_action);

  processor.EnqueueAction(&post_action);
  processor.EnqueueAction(&collector_action);
  delegate.RunProcessor(&processor);
  EXPECT_TRUE(delegate.ran());
  EXPECT_EQ(kActionCodeNewPCRPolicyVerificationError, delegate.code());
}

TEST(PCRPolicyPostActionTest, MissingPCRPolicyTest) {
  ActionProcessor processor;
  ActionTestDelegate<PCRPolicyPostAction> delegate;

  ObjectFeederAction<InstallPlan> feeder_action;
  InstallPlan install_plan;
  install_plan.pcr_policy_path = "/no/such/file";
  feeder_action.set_obj(install_plan);
  PCRPolicyPostAction post_action(nullptr, nullptr);
  ObjectCollectorAction<InstallPlan> collector_action;

  BondActions(&post_action, &collector_action);

  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&post_action);
  processor.EnqueueAction(&collector_action);
  delegate.RunProcessor(&processor);
  EXPECT_TRUE(delegate.ran());
  EXPECT_EQ(kActionCodeNewPCRPolicyVerificationError, delegate.code());
}

}  // namespace chromeos_update_engine
