// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <string>

#include "base/file_util.h"
#include "gtest/gtest.h"
#include "update_engine/install_plan.h"
#include "update_engine/mock_system_state.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

class OmahaRequestParamsTest : public ::testing::Test {
 public:
  OmahaRequestParamsTest() : params_(NULL) {}

 protected:
  // Return true iff the OmahaRequestParams::Init succeeded. If
  // out is non-NULL, it's set w/ the generated data.
  bool DoTest(OmahaRequestParams* out, const string& app_version,
              const string& omaha_url);

  virtual void SetUp() {
    ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir + "/etc"));
    ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir +
                        utils::kStatefulPartition + "/etc"));
    // Create a fresh copy of the params for each test, so there's no
    // unintended reuse of state across tests.
    MockSystemState mock_system_state;
    OmahaRequestParams new_params(&mock_system_state);
    params_ = new_params;
    params_.set_root(string("./") + kTestDir);
    params_.SetLockDown(false);
  }

  virtual void TearDown() {
    EXPECT_EQ(0, System(string("rm -rf ") + kTestDir));
  }

  OmahaRequestParams params_;

  static const string kTestDir;
};

const string OmahaRequestParamsTest::kTestDir =
    "omaha_request_params-test";

bool OmahaRequestParamsTest::DoTest(OmahaRequestParams* out,
                                    const string& app_version,
                                    const string& omaha_url) {
  bool success = params_.Init(app_version, omaha_url, false);
  if (out)
    *out = params_;
  return success;
}

namespace {
string GetMachineType() {
  string machine_type;
  if (!utils::ReadPipe("uname -m", &machine_type))
    return "";
  // Strip anything from the first newline char.
  size_t newline_pos = machine_type.find('\n');
  if (newline_pos != string::npos)
    machine_type.erase(newline_pos);
  return machine_type;
}
}  // namespace {}

TEST_F(OmahaRequestParamsTest, SimpleTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=arm-generic\n"
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRACK=dev-channel\n"
      "COREOS_AUSERVER=http://www.google.com"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("Chrome OS", out.os_platform());
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp());
  EXPECT_EQ("arm-generic", out.os_board());
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id());
  EXPECT_EQ("0.2.2.3", out.app_version());
  EXPECT_EQ("en-US", out.app_lang());
  EXPECT_EQ("", out.hwid());
  EXPECT_TRUE(out.delta_okay());
  EXPECT_EQ("dev-channel", out.target_channel());
  EXPECT_EQ("http://www.google.com", out.update_url());
}

TEST_F(OmahaRequestParamsTest, AppIDTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=arm-generic\n"
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRACK=dev-channel\n"
      "COREOS_RELEASE_APPID={58c35cef-9d30-476e-9098-ce20377d535d}\n"
      "COREOS_AUSERVER=http://www.google.com"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("Chrome OS", out.os_platform());
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp());
  EXPECT_EQ("arm-generic", out.os_board());
  EXPECT_EQ("{58c35cef-9d30-476e-9098-ce20377d535d}", out.app_id());
  EXPECT_EQ("0.2.2.3", out.app_version());
  EXPECT_EQ("en-US", out.app_lang());
  EXPECT_EQ("", out.hwid());
  EXPECT_TRUE(out.delta_okay());
  EXPECT_EQ("dev-channel", out.target_channel());
  EXPECT_EQ("http://www.google.com", out.update_url());
}

TEST_F(OmahaRequestParamsTest, MissingChannelTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRXCK=dev-channel"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("Chrome OS", out.os_platform());
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp());
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id());
  EXPECT_EQ("0.2.2.3", out.app_version());
  EXPECT_EQ("en-US", out.app_lang());
  EXPECT_EQ("", out.target_channel());
}

TEST_F(OmahaRequestParamsTest, ConfusingReleaseTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_FOO=COREOS_RELEASE_VERSION=1.2.3.4\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRXCK=dev-channel"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("Chrome OS", out.os_platform());
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp());
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id());
  EXPECT_EQ("0.2.2.3", out.app_version());
  EXPECT_EQ("en-US", out.app_lang());
  EXPECT_EQ("", out.target_channel());
}

TEST_F(OmahaRequestParamsTest, MissingVersionTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=arm-generic\n"
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_TRACK=dev-channel"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("Chrome OS", out.os_platform());
  EXPECT_EQ(string("_") + GetMachineType(), out.os_sp());
  EXPECT_EQ("arm-generic", out.os_board());
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id());
  EXPECT_EQ("", out.app_version());
  EXPECT_EQ("en-US", out.app_lang());
  EXPECT_TRUE(out.delta_okay());
  EXPECT_EQ("dev-channel", out.target_channel());
}

TEST_F(OmahaRequestParamsTest, ForceVersionTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=arm-generic\n"
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_TRACK=dev-channel"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out, "ForcedVersion", ""));
  EXPECT_EQ("Chrome OS", out.os_platform());
  EXPECT_EQ(string("ForcedVersion_") + GetMachineType(), out.os_sp());
  EXPECT_EQ("arm-generic", out.os_board());
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id());
  EXPECT_EQ("ForcedVersion", out.app_version());
  EXPECT_EQ("en-US", out.app_lang());
  EXPECT_TRUE(out.delta_okay());
  EXPECT_EQ("dev-channel", out.target_channel());
}

TEST_F(OmahaRequestParamsTest, ForcedURLTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=arm-generic\n"
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRACK=dev-channel"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out, "", "http://forced.google.com"));
  EXPECT_EQ("Chrome OS", out.os_platform());
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp());
  EXPECT_EQ("arm-generic", out.os_board());
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id());
  EXPECT_EQ("0.2.2.3", out.app_version());
  EXPECT_EQ("en-US", out.app_lang());
  EXPECT_TRUE(out.delta_okay());
  EXPECT_EQ("dev-channel", out.target_channel());
  EXPECT_EQ("http://forced.google.com", out.update_url());
}

TEST_F(OmahaRequestParamsTest, MissingURLTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=arm-generic\n"
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRACK=dev-channel"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("Chrome OS", out.os_platform());
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp());
  EXPECT_EQ("arm-generic", out.os_board());
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id());
  EXPECT_EQ("0.2.2.3", out.app_version());
  EXPECT_EQ("en-US", out.app_lang());
  EXPECT_TRUE(out.delta_okay());
  EXPECT_EQ("dev-channel", out.target_channel());
  EXPECT_EQ(kProductionOmahaUrl, out.update_url());
}

TEST_F(OmahaRequestParamsTest, NoDeltasTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_FOO=COREOS_RELEASE_VERSION=1.2.3.4\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRXCK=dev-channel"));
  ASSERT_TRUE(WriteFileString(kTestDir + "/.nodelta", ""));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_FALSE(out.delta_okay());
}

TEST_F(OmahaRequestParamsTest, OverrideTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=arm-generic\n"
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRACK=dev-channel\n"
      "COREOS_AUSERVER=http://www.google.com"));
  ASSERT_TRUE(WriteFileString(
      kTestDir + utils::kStatefulPartition + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=x86-generic\n"
      "COREOS_RELEASE_TRACK=beta-channel\n"
      "COREOS_AUSERVER=https://www.google.com"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("Chrome OS", out.os_platform());
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp());
  EXPECT_EQ("x86-generic", out.os_board());
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id());
  EXPECT_EQ("0.2.2.3", out.app_version());
  EXPECT_EQ("en-US", out.app_lang());
  EXPECT_EQ("", out.hwid());
  EXPECT_FALSE(out.delta_okay());
  EXPECT_EQ("beta-channel", out.target_channel());
  EXPECT_EQ("https://www.google.com", out.update_url());
}

TEST_F(OmahaRequestParamsTest, OverrideLockDownTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=arm-generic\n"
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRACK=dev-channel\n"
      "COREOS_AUSERVER=https://www.google.com"));
  ASSERT_TRUE(WriteFileString(
      kTestDir + utils::kStatefulPartition + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=x86-generic\n"
      "COREOS_RELEASE_TRACK=stable-channel\n"
      "COREOS_AUSERVER=http://www.google.com"));
  params_.SetLockDown(true);
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("arm-generic", out.os_board());
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id());
  EXPECT_EQ("0.2.2.3", out.app_version());
  EXPECT_EQ("", out.hwid());
  EXPECT_FALSE(out.delta_okay());
  EXPECT_EQ("stable-channel", out.target_channel());
  EXPECT_EQ("https://www.google.com", out.update_url());
}

TEST_F(OmahaRequestParamsTest, OverrideSameChannelTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=arm-generic\n"
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRACK=dev-channel\n"
      "COREOS_AUSERVER=http://www.google.com"));
  ASSERT_TRUE(WriteFileString(
      kTestDir + utils::kStatefulPartition + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=x86-generic\n"
      "COREOS_RELEASE_TRACK=dev-channel"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("x86-generic", out.os_board());
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id());
  EXPECT_EQ("0.2.2.3", out.app_version());
  EXPECT_EQ("", out.hwid());
  EXPECT_TRUE(out.delta_okay());
  EXPECT_EQ("dev-channel", out.target_channel());
  EXPECT_EQ("http://www.google.com", out.update_url());
}

TEST_F(OmahaRequestParamsTest, SetTargetChannelTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=arm-generic\n"
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRACK=dev-channel\n"
      "COREOS_AUSERVER=http://www.google.com"));
  {
    MockSystemState mock_system_state;
    OmahaRequestParams params(&mock_system_state);
    params.set_root(string("./") + kTestDir);
    params.SetLockDown(false);
    EXPECT_TRUE(params.Init("", "", false));
    params.SetTargetChannel("canary-channel", false);
    EXPECT_FALSE(params.is_powerwash_allowed());
  }
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("canary-channel", out.target_channel());
  EXPECT_FALSE(out.is_powerwash_allowed());
}

TEST_F(OmahaRequestParamsTest, SetIsPowerwashAllowedTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=arm-generic\n"
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRACK=dev-channel\n"
      "COREOS_AUSERVER=http://www.google.com"));
  {
    MockSystemState mock_system_state;
    OmahaRequestParams params(&mock_system_state);
    params.set_root(string("./") + kTestDir);
    params.SetLockDown(false);
    EXPECT_TRUE(params.Init("", "", false));
    params.SetTargetChannel("canary-channel", true);
    EXPECT_TRUE(params.is_powerwash_allowed());
  }
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("canary-channel", out.target_channel());
  EXPECT_TRUE(out.is_powerwash_allowed());
}

TEST_F(OmahaRequestParamsTest, SetTargetChannelInvalidTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=arm-generic\n"
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRACK=dev-channel\n"
      "COREOS_AUSERVER=http://www.google.com"));
  {
    MockSystemState mock_system_state;
    OmahaRequestParams params(&mock_system_state);
    params.set_root(string("./") + kTestDir);
    params.SetLockDown(true);
    EXPECT_TRUE(params.Init("", "", false));
    params.SetTargetChannel("dogfood-channel", true);
    EXPECT_FALSE(params.is_powerwash_allowed());
  }
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("arm-generic", out.os_board());
  EXPECT_EQ("dev-channel", out.target_channel());
  EXPECT_FALSE(out.is_powerwash_allowed());
}

TEST_F(OmahaRequestParamsTest, IsValidChannelTest) {
  params_.SetLockDown(false);
  EXPECT_TRUE(params_.IsValidChannel("canary-channel"));
  EXPECT_TRUE(params_.IsValidChannel("stable-channel"));
  EXPECT_TRUE(params_.IsValidChannel("beta-channel"));
  EXPECT_TRUE(params_.IsValidChannel("dev-channel"));
  EXPECT_FALSE(params_.IsValidChannel("testimage-channel"));
  EXPECT_FALSE(params_.IsValidChannel("dogfood-channel"));
  EXPECT_FALSE(params_.IsValidChannel("some-channel"));
  EXPECT_FALSE(params_.IsValidChannel(""));
}

TEST_F(OmahaRequestParamsTest, ValidChannelTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=arm-generic\n"
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRACK=dev-channel\n"
      "COREOS_AUSERVER=http://www.google.com"));
  params_.SetLockDown(true);
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("Chrome OS", out.os_platform());
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp());
  EXPECT_EQ("arm-generic", out.os_board());
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id());
  EXPECT_EQ("0.2.2.3", out.app_version());
  EXPECT_EQ("en-US", out.app_lang());
  EXPECT_EQ("", out.hwid());
  EXPECT_TRUE(out.delta_okay());
  EXPECT_EQ("dev-channel", out.target_channel());
  EXPECT_EQ("http://www.google.com", out.update_url());
}

TEST_F(OmahaRequestParamsTest, SetTargetChannelWorks) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=arm-generic\n"
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRACK=dev-channel\n"
      "COREOS_AUSERVER=http://www.google.com"));
  params_.SetLockDown(false);

  // Check LSB value is used by default when SetTargetChannel is not called.
  params_.Init("", "", false);
  EXPECT_EQ("dev-channel", params_.target_channel());

  // When an invalid value is set, it should be ignored and the
  // value from lsb-release should be used instead.
  params_.Init("", "", false);
  EXPECT_FALSE(params_.SetTargetChannel("invalid-channel", false));
  EXPECT_EQ("dev-channel", params_.target_channel());

  // When set to a valid value, it should take effect.
  params_.Init("", "", false);
  EXPECT_TRUE(params_.SetTargetChannel("beta-channel", true));
  EXPECT_EQ("beta-channel", params_.target_channel());

  // When set to the same value, it should be idempotent.
  params_.Init("", "", false);
  EXPECT_TRUE(params_.SetTargetChannel("beta-channel", true));
  EXPECT_EQ("beta-channel", params_.target_channel());

  // When set to a valid value while a change is already pending, it should
  // fail.
  params_.Init("", "", false);
  EXPECT_FALSE(params_.SetTargetChannel("stable-channel", true));
  EXPECT_EQ("beta-channel", params_.target_channel());
}

TEST_F(OmahaRequestParamsTest, ChannelIndexTest) {
  int canary = params_.GetChannelIndex("canary-channel");
  int dev = params_.GetChannelIndex("dev-channel");
  int beta = params_.GetChannelIndex("beta-channel");
  int stable = params_.GetChannelIndex("stable-channel");
  EXPECT_LE(canary, dev);
  EXPECT_LE(dev, beta);
  EXPECT_LE(beta, stable);

  // testimage-channel or other names are not recognized, so index will be -1.
  int testimage = params_.GetChannelIndex("testimage-channel");
  int bogus = params_.GetChannelIndex("bogus-channel");
  EXPECT_EQ(-1, testimage);
  EXPECT_EQ(-1, bogus);
}

TEST_F(OmahaRequestParamsTest, ToMoreStableChannelFlagTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=arm-generic\n"
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRACK=canary-channel\n"
      "COREOS_AUSERVER=http://www.google.com"));
  ASSERT_TRUE(WriteFileString(
      kTestDir + utils::kStatefulPartition + "/etc/lsb-release",
      "COREOS_RELEASE_BOARD=x86-generic\n"
      "COREOS_RELEASE_TRACK=stable-channel\n"
      "COREOS_AUSERVER=https://www.google.com"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("https://www.google.com", out.update_url());
  EXPECT_FALSE(out.delta_okay());
  EXPECT_EQ("stable-channel", out.target_channel());
  EXPECT_TRUE(out.to_more_stable_channel());
}

TEST_F(OmahaRequestParamsTest, ShouldLockDownTest) {
  EXPECT_FALSE(params_.ShouldLockDown());
}

}  // namespace chromeos_update_engine
