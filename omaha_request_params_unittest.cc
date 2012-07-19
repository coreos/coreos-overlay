// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <string>

#include "base/file_util.h"
#include "gtest/gtest.h"
#include "update_engine/install_plan.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

class OmahaRequestDeviceParamsTest : public ::testing::Test {
 protected:
  // Return true iff the OmahaRequestDeviceParams::Init succeeded. If
  // out is non-NULL, it's set w/ the generated data.
  bool DoTest(OmahaRequestParams* out, const string& app_version,
              const string& omaha_url);

  virtual void SetUp() {
    ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir + "/etc"));
    ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir +
                        utils::kStatefulPartition + "/etc"));
    params_.set_root(string("./") + kTestDir);
    params_.SetLockDown(false);
  }

  virtual void TearDown() {
    EXPECT_EQ(0, System(string("rm -rf ") + kTestDir));
  }

  OmahaRequestDeviceParams params_;

  static const string kTestDir;
};

const string OmahaRequestDeviceParamsTest::kTestDir =
    "omaha_request_device_params-test";

bool OmahaRequestDeviceParamsTest::DoTest(OmahaRequestParams* out,
                                          const string& app_version,
                                          const string& omaha_url) {
  bool success = params_.Init(app_version, omaha_url, "");
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

TEST_F(OmahaRequestDeviceParamsTest, SimpleTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=arm-generic\n"
      "CHROMEOS_RELEASE_FOO=bar\n"
      "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
      "CHROMEOS_RELEASE_TRACK=footrack\n"
      "CHROMEOS_AUSERVER=http://www.google.com"));
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("Chrome OS", out.os_platform);
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp);
  EXPECT_EQ("arm-generic", out.os_board);
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id);
  EXPECT_EQ("0.2.2.3", out.app_version);
  EXPECT_EQ("en-US", out.app_lang);
  EXPECT_EQ("", out.hardware_class);
  EXPECT_TRUE(out.delta_okay);
  EXPECT_EQ("footrack", out.app_track);
  EXPECT_EQ("http://www.google.com", out.update_url);
}

TEST_F(OmahaRequestDeviceParamsTest, AppIDTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=arm-generic\n"
      "CHROMEOS_RELEASE_FOO=bar\n"
      "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
      "CHROMEOS_RELEASE_TRACK=footrack\n"
      "CHROMEOS_RELEASE_APPID={58c35cef-9d30-476e-9098-ce20377d535d}\n"
      "CHROMEOS_AUSERVER=http://www.google.com"));
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("Chrome OS", out.os_platform);
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp);
  EXPECT_EQ("arm-generic", out.os_board);
  EXPECT_EQ("{58c35cef-9d30-476e-9098-ce20377d535d}", out.app_id);
  EXPECT_EQ("0.2.2.3", out.app_version);
  EXPECT_EQ("en-US", out.app_lang);
  EXPECT_EQ("", out.hardware_class);
  EXPECT_TRUE(out.delta_okay);
  EXPECT_EQ("footrack", out.app_track);
  EXPECT_EQ("http://www.google.com", out.update_url);
}

TEST_F(OmahaRequestDeviceParamsTest, MissingTrackTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_FOO=bar\n"
      "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
      "CHROMEOS_RELEASE_TRXCK=footrack"));
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("Chrome OS", out.os_platform);
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp);
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id);
  EXPECT_EQ("0.2.2.3", out.app_version);
  EXPECT_EQ("en-US", out.app_lang);
  EXPECT_EQ("", out.app_track);
}

TEST_F(OmahaRequestDeviceParamsTest, ConfusingReleaseTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_FOO=CHROMEOS_RELEASE_VERSION=1.2.3.4\n"
      "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
      "CHROMEOS_RELEASE_TRXCK=footrack"));
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("Chrome OS", out.os_platform);
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp);
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id);
  EXPECT_EQ("0.2.2.3", out.app_version);
  EXPECT_EQ("en-US", out.app_lang);
  EXPECT_EQ("", out.app_track);
}

TEST_F(OmahaRequestDeviceParamsTest, MissingVersionTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=arm-generic\n"
      "CHROMEOS_RELEASE_FOO=bar\n"
      "CHROMEOS_RELEASE_TRACK=footrack"));
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("Chrome OS", out.os_platform);
  EXPECT_EQ(string("_") + GetMachineType(), out.os_sp);
  EXPECT_EQ("arm-generic", out.os_board);
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id);
  EXPECT_EQ("", out.app_version);
  EXPECT_EQ("en-US", out.app_lang);
  EXPECT_TRUE(out.delta_okay);
  EXPECT_EQ("footrack", out.app_track);
}

TEST_F(OmahaRequestDeviceParamsTest, ForceVersionTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=arm-generic\n"
      "CHROMEOS_RELEASE_FOO=bar\n"
      "CHROMEOS_RELEASE_TRACK=footrack"));
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out, "ForcedVersion", ""));
  EXPECT_EQ("Chrome OS", out.os_platform);
  EXPECT_EQ(string("ForcedVersion_") + GetMachineType(), out.os_sp);
  EXPECT_EQ("arm-generic", out.os_board);
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id);
  EXPECT_EQ("ForcedVersion", out.app_version);
  EXPECT_EQ("en-US", out.app_lang);
  EXPECT_TRUE(out.delta_okay);
  EXPECT_EQ("footrack", out.app_track);
}

TEST_F(OmahaRequestDeviceParamsTest, ForcedURLTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=arm-generic\n"
      "CHROMEOS_RELEASE_FOO=bar\n"
      "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
      "CHROMEOS_RELEASE_TRACK=footrack"));
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out, "", "http://forced.google.com"));
  EXPECT_EQ("Chrome OS", out.os_platform);
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp);
  EXPECT_EQ("arm-generic", out.os_board);
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id);
  EXPECT_EQ("0.2.2.3", out.app_version);
  EXPECT_EQ("en-US", out.app_lang);
  EXPECT_TRUE(out.delta_okay);
  EXPECT_EQ("footrack", out.app_track);
  EXPECT_EQ("http://forced.google.com", out.update_url);
}

TEST_F(OmahaRequestDeviceParamsTest, MissingURLTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=arm-generic\n"
      "CHROMEOS_RELEASE_FOO=bar\n"
      "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
      "CHROMEOS_RELEASE_TRACK=footrack"));
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("Chrome OS", out.os_platform);
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp);
  EXPECT_EQ("arm-generic", out.os_board);
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id);
  EXPECT_EQ("0.2.2.3", out.app_version);
  EXPECT_EQ("en-US", out.app_lang);
  EXPECT_TRUE(out.delta_okay);
  EXPECT_EQ("footrack", out.app_track);
  EXPECT_EQ(OmahaRequestParams::kUpdateUrl, out.update_url);
}

TEST_F(OmahaRequestDeviceParamsTest, NoDeltasTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_FOO=CHROMEOS_RELEASE_VERSION=1.2.3.4\n"
      "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
      "CHROMEOS_RELEASE_TRXCK=footrack"));
  ASSERT_TRUE(WriteFileString(kTestDir + "/.nodelta", ""));
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_FALSE(out.delta_okay);
}

TEST_F(OmahaRequestDeviceParamsTest, OverrideTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=arm-generic\n"
      "CHROMEOS_RELEASE_FOO=bar\n"
      "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
      "CHROMEOS_RELEASE_TRACK=footrack\n"
      "CHROMEOS_AUSERVER=http://www.google.com"));
  ASSERT_TRUE(WriteFileString(
      kTestDir + utils::kStatefulPartition + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=x86-generic\n"
      "CHROMEOS_RELEASE_TRACK=bartrack\n"
      "CHROMEOS_AUSERVER=https://www.google.com"));
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("Chrome OS", out.os_platform);
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp);
  EXPECT_EQ("x86-generic", out.os_board);
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id);
  EXPECT_EQ("0.2.2.3", out.app_version);
  EXPECT_EQ("en-US", out.app_lang);
  EXPECT_EQ("", out.hardware_class);
  EXPECT_FALSE(out.delta_okay);
  EXPECT_EQ("bartrack", out.app_track);
  EXPECT_EQ("https://www.google.com", out.update_url);
}

TEST_F(OmahaRequestDeviceParamsTest, OverrideLockDownTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=arm-generic\n"
      "CHROMEOS_RELEASE_FOO=bar\n"
      "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
      "CHROMEOS_RELEASE_TRACK=footrack\n"
      "CHROMEOS_AUSERVER=https://www.google.com"));
  ASSERT_TRUE(WriteFileString(
      kTestDir + utils::kStatefulPartition + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=x86-generic\n"
      "CHROMEOS_RELEASE_TRACK=dev-channel\n"
      "CHROMEOS_AUSERVER=http://www.google.com"));
  params_.SetLockDown(true);
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("arm-generic", out.os_board);
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id);
  EXPECT_EQ("0.2.2.3", out.app_version);
  EXPECT_EQ("", out.hardware_class);
  EXPECT_FALSE(out.delta_okay);
  EXPECT_EQ("dev-channel", out.app_track);
  EXPECT_EQ("https://www.google.com", out.update_url);
}

TEST_F(OmahaRequestDeviceParamsTest, OverrideSameTrackTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=arm-generic\n"
      "CHROMEOS_RELEASE_FOO=bar\n"
      "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
      "CHROMEOS_RELEASE_TRACK=footrack\n"
      "CHROMEOS_AUSERVER=http://www.google.com"));
  ASSERT_TRUE(WriteFileString(
      kTestDir + utils::kStatefulPartition + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=x86-generic\n"
      "CHROMEOS_RELEASE_TRACK=footrack"));
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("x86-generic", out.os_board);
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id);
  EXPECT_EQ("0.2.2.3", out.app_version);
  EXPECT_EQ("", out.hardware_class);
  EXPECT_TRUE(out.delta_okay);
  EXPECT_EQ("footrack", out.app_track);
  EXPECT_EQ("http://www.google.com", out.update_url);
}

TEST_F(OmahaRequestDeviceParamsTest, SetTrackSimpleTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=arm-generic\n"
      "CHROMEOS_RELEASE_FOO=bar\n"
      "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
      "CHROMEOS_RELEASE_TRACK=footrack\n"
      "CHROMEOS_AUSERVER=http://www.google.com"));
  {
    OmahaRequestDeviceParams params;
    params.set_root(string("./") + kTestDir);
    params.SetLockDown(false);
    EXPECT_TRUE(params.Init("", "", ""));
    params.SetTrack("zootrack");
  }
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("arm-generic", out.os_board);
  EXPECT_EQ("zootrack", out.app_track);
}

TEST_F(OmahaRequestDeviceParamsTest, SetTrackPreserveTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=arm-generic\n"
      "CHROMEOS_RELEASE_FOO=bar\n"
      "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
      "CHROMEOS_RELEASE_TRACK=footrack\n"
      "CHROMEOS_AUSERVER=http://www.google.com"));
  ASSERT_TRUE(WriteFileString(
      kTestDir + utils::kStatefulPartition + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=x86-generic\n"
      "CHROMEOS_RELEASE_TRACK=bartrack"));
  {
    OmahaRequestDeviceParams params;
    params.set_root(string("./") + kTestDir);
    params.SetLockDown(false);
    EXPECT_TRUE(params.Init("", "", ""));
    params.SetTrack("zootrack");
  }
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("x86-generic", out.os_board);
  EXPECT_EQ("zootrack", out.app_track);
}

TEST_F(OmahaRequestDeviceParamsTest, SetTrackInvalidTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=arm-generic\n"
      "CHROMEOS_RELEASE_FOO=bar\n"
      "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
      "CHROMEOS_RELEASE_TRACK=footrack\n"
      "CHROMEOS_AUSERVER=http://www.google.com"));
  {
    OmahaRequestDeviceParams params;
    params.set_root(string("./") + kTestDir);
    params.SetLockDown(true);
    EXPECT_TRUE(params.Init("", "", ""));
    params.SetTrack("zootrack");
  }
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("arm-generic", out.os_board);
  EXPECT_EQ("footrack", out.app_track);
}

TEST_F(OmahaRequestDeviceParamsTest, IsValidTrackTest) {
  params_.SetLockDown(true);
  EXPECT_TRUE(params_.IsValidTrack("canary-channel"));
  EXPECT_TRUE(params_.IsValidTrack("stable-channel"));
  EXPECT_TRUE(params_.IsValidTrack("beta-channel"));
  EXPECT_TRUE(params_.IsValidTrack("dev-channel"));
  EXPECT_TRUE(params_.IsValidTrack("dogfood-channel"));
  EXPECT_FALSE(params_.IsValidTrack("some-channel"));
  EXPECT_FALSE(params_.IsValidTrack(""));
  params_.SetLockDown(false);
  EXPECT_TRUE(params_.IsValidTrack("canary-channel"));
  EXPECT_TRUE(params_.IsValidTrack("stable-channel"));
  EXPECT_TRUE(params_.IsValidTrack("beta-channel"));
  EXPECT_TRUE(params_.IsValidTrack("dev-channel"));
  EXPECT_TRUE(params_.IsValidTrack("dogfood-channel"));
  EXPECT_TRUE(params_.IsValidTrack("some-channel"));
  EXPECT_TRUE(params_.IsValidTrack(""));
}

TEST_F(OmahaRequestDeviceParamsTest, ValidTrackTest) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=arm-generic\n"
      "CHROMEOS_RELEASE_FOO=bar\n"
      "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
      "CHROMEOS_RELEASE_TRACK=dev-channel\n"
      "CHROMEOS_AUSERVER=http://www.google.com"));
  params_.SetLockDown(true);
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("Chrome OS", out.os_platform);
  EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp);
  EXPECT_EQ("arm-generic", out.os_board);
  EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id);
  EXPECT_EQ("0.2.2.3", out.app_version);
  EXPECT_EQ("en-US", out.app_lang);
  EXPECT_EQ("", out.hardware_class);
  EXPECT_TRUE(out.delta_okay);
  EXPECT_EQ("dev-channel", out.app_track);
  EXPECT_EQ("http://www.google.com", out.update_url);
}

TEST_F(OmahaRequestDeviceParamsTest, ChannelSpecified) {
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_BOARD=arm-generic\n"
      "CHROMEOS_RELEASE_FOO=bar\n"
      "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
      "CHROMEOS_RELEASE_TRACK=dev-channel\n"
      "CHROMEOS_AUSERVER=http://www.google.com"));
  params_.SetLockDown(true);
  // Passed-in value for release channel should be used.
  params_.Init("", "", "beta-channel");
  EXPECT_EQ("beta-channel", params_.app_track);

  // When passed-in value is invalid, value from lsb-release should be used.
  params_.Init("", "", "foo-channel");
  EXPECT_EQ("dev-channel", params_.app_track);

  // When passed-in value is empty, value from lsb-release should be used.
  params_.Init("", "", "");
  EXPECT_EQ("dev-channel", params_.app_track);
}

TEST_F(OmahaRequestDeviceParamsTest, ShouldLockDownTest) {
  EXPECT_FALSE(params_.ShouldLockDown());
}

}  // namespace chromeos_update_engine
