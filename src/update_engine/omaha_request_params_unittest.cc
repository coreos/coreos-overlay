// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <string>

#include <gtest/gtest.h>

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
  bool DoTest(OmahaRequestParams* out);

  virtual void SetUp() {
    ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir + "/usr/share/coreos"));
    // Create a fresh copy of the params for each test, so there's no
    // unintended reuse of state across tests.
    MockSystemState mock_system_state;
    OmahaRequestParams new_params(&mock_system_state);
    params_ = new_params;
    params_.set_root(string("./") + kTestDir);
  }

  virtual void TearDown() {
    EXPECT_EQ(0, System(string("rm -rf ") + kTestDir));
  }

  OmahaRequestParams params_;

  static const string kTestDir;
};

const string OmahaRequestParamsTest::kTestDir =
    "omaha_request_params-test";

bool OmahaRequestParamsTest::DoTest(OmahaRequestParams* out) {
  bool success = params_.Init(false);
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
      kTestDir + "/usr/share/coreos/release",
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "GROUP=dev-channel\n"
      "SERVER=http://www.google.com"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out));
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp());
  EXPECT_EQ("{e96281a6-d1af-4bde-9a0a-97b76e56dc57}", out.app_id());
  EXPECT_EQ("0.2.2.3", out.app_version());
  EXPECT_EQ("en-US", out.app_lang());
  EXPECT_EQ("", out.hwid());
  EXPECT_FALSE(out.delta_okay());
  EXPECT_EQ("dev-channel", out.app_channel());
  EXPECT_EQ("http://www.google.com", out.update_url());
}

TEST_F(OmahaRequestParamsTest, AppIDTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/usr/share/coreos/release",
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "GROUP=dev-channel\n"
      "COREOS_RELEASE_APPID={58c35cef-9d30-476e-9098-ce20377d535d}\n"
      "SERVER=http://www.google.com"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out));
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp());
  EXPECT_EQ("{58c35cef-9d30-476e-9098-ce20377d535d}", out.app_id());
  EXPECT_EQ("0.2.2.3", out.app_version());
  EXPECT_EQ("en-US", out.app_lang());
  EXPECT_EQ("", out.hwid());
  EXPECT_FALSE(out.delta_okay());
  EXPECT_EQ("dev-channel", out.app_channel());
  EXPECT_EQ("http://www.google.com", out.update_url());
}

TEST_F(OmahaRequestParamsTest, MissingChannelTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/usr/share/coreos/release",
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRXCK=dev-channel"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out));
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp());
  EXPECT_EQ("{e96281a6-d1af-4bde-9a0a-97b76e56dc57}", out.app_id());
  EXPECT_EQ("0.2.2.3", out.app_version());
  EXPECT_EQ("en-US", out.app_lang());
  EXPECT_EQ("stable", out.app_channel());
}

TEST_F(OmahaRequestParamsTest, ConfusingReleaseTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/usr/share/coreos/release",
      "COREOS_RELEASE_FOO=COREOS_RELEASE_VERSION=1.2.3.4\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "COREOS_RELEASE_TRXCK=dev-channel"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out));
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp());
  EXPECT_EQ("{e96281a6-d1af-4bde-9a0a-97b76e56dc57}", out.app_id());
  EXPECT_EQ("0.2.2.3", out.app_version());
  EXPECT_EQ("en-US", out.app_lang());
  EXPECT_EQ("stable", out.app_channel());
}

TEST_F(OmahaRequestParamsTest, MissingVersionTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/usr/share/coreos/release",
      "COREOS_RELEASE_FOO=bar\n"
      "GROUP=dev-channel"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out));
  EXPECT_EQ(string("_") + GetMachineType(), out.os_sp());
  EXPECT_EQ("{e96281a6-d1af-4bde-9a0a-97b76e56dc57}", out.app_id());
  EXPECT_EQ("", out.app_version());
  EXPECT_EQ("en-US", out.app_lang());
  EXPECT_FALSE(out.delta_okay());
  EXPECT_EQ("dev-channel", out.app_channel());
}

TEST_F(OmahaRequestParamsTest, MissingURLTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/usr/share/coreos/release",
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "GROUP=dev-channel"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out));
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp());
  EXPECT_EQ("{e96281a6-d1af-4bde-9a0a-97b76e56dc57}", out.app_id());
  EXPECT_EQ("0.2.2.3", out.app_version());
  EXPECT_EQ("en-US", out.app_lang());
  EXPECT_FALSE(out.delta_okay());
  EXPECT_EQ("dev-channel", out.app_channel());
  EXPECT_EQ(kProductionOmahaUrl, out.update_url());
}

TEST_F(OmahaRequestParamsTest, ValidChannelTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/usr/share/coreos/release",
      "COREOS_RELEASE_FOO=bar\n"
      "COREOS_RELEASE_VERSION=0.2.2.3\n"
      "GROUP=dev-channel\n"
      "SERVER=http://www.google.com"));
  MockSystemState mock_system_state;
  OmahaRequestParams out(&mock_system_state);
  EXPECT_TRUE(DoTest(&out));
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp());
  EXPECT_EQ("{e96281a6-d1af-4bde-9a0a-97b76e56dc57}", out.app_id());
  EXPECT_EQ("0.2.2.3", out.app_version());
  EXPECT_EQ("en-US", out.app_lang());
  EXPECT_EQ("", out.hwid());
  EXPECT_FALSE(out.delta_okay());
  EXPECT_EQ("dev-channel", out.app_channel());
  EXPECT_EQ("http://www.google.com", out.update_url());
}

}  // namespace chromeos_update_engine
