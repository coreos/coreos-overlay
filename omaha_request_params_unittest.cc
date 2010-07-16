// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>
#include <gtest/gtest.h>
#include "update_engine/install_plan.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

class OmahaRequestDeviceParamsTest : public ::testing::Test {
 public:
  // Return true iff the OmahaRequestDeviceParams::Init succeeded. If
  // out is non-NULL, it's set w/ the generated data.
  bool DoTest(OmahaRequestParams* out);
  static const string kTestDir;
};

const string OmahaRequestDeviceParamsTest::kTestDir =
    "omaha_request_device_params-test";

bool OmahaRequestDeviceParamsTest::DoTest(OmahaRequestParams* out) {
  OmahaRequestDeviceParams params;
  params.set_root(string("./") + kTestDir);
  bool success = params.Init();
  if (out)
    *out = params;
  return success;
}

namespace {
bool IsHexDigit(char c) {
  return ((c >= '0') && (c <= '9')) ||
      ((c >= 'a') && (c <= 'f')) ||
      ((c >= 'A') && (c <= 'F'));
}

// Returns true iff str is formatted as a GUID. Example GUID:
// "{2251FFAD-DBAB-4E53-8B3A-18F98BB4EB80}"
bool IsValidGuid(const string& str) {
  TEST_AND_RETURN_FALSE(str.size() == 38);
  TEST_AND_RETURN_FALSE((*str.begin() == '{') && (*str.rbegin() == '}'));
  for (string::size_type i = 1; i < (str.size() - 1); ++i) {
    if ((i == 9) || (i == 14) || (i == 19) || (i == 24)) {
      TEST_AND_RETURN_FALSE(str[i] == '-');
    } else {
      TEST_AND_RETURN_FALSE(IsHexDigit(str[i]));
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

TEST_F(OmahaRequestDeviceParamsTest, SimpleTest) {
  ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir + "/etc"));
  ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir +
                      utils::kStatefulPartition + "/etc"));
  {
    ASSERT_TRUE(WriteFileString(
        kTestDir + "/etc/lsb-release",
        "CHROMEOS_RELEASE_BOARD=arm-generic\n"
        "CHROMEOS_RELEASE_FOO=bar\n"
        "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
        "CHROMEOS_RELEASE_TRACK=footrack"));
    OmahaRequestParams out;
    EXPECT_TRUE(DoTest(&out));
    EXPECT_TRUE(IsValidGuid(out.machine_id)) << "id: " << out.machine_id;
    // for now we're just using the machine id here
    EXPECT_TRUE(IsValidGuid(out.user_id)) << "id: " << out.user_id;
    EXPECT_EQ("Chrome OS", out.os_platform);
    EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp);
    EXPECT_EQ("arm-generic", out.os_board);
    EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id);
    EXPECT_EQ("0.2.2.3", out.app_version);
    EXPECT_EQ("en-US", out.app_lang);
    EXPECT_TRUE(out.delta_okay);
    EXPECT_EQ("footrack", out.app_track);
  }
  EXPECT_EQ(0, System(string("rm -rf ") + kTestDir));
}

TEST_F(OmahaRequestDeviceParamsTest, MissingTrackTest) {
  ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir + "/etc"));
  ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir +
                      utils::kStatefulPartition + "/etc"));
  {
    ASSERT_TRUE(WriteFileString(
        kTestDir + "/etc/lsb-release",
        "CHROMEOS_RELEASE_FOO=bar\n"
        "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
        "CHROMEOS_RELEASE_TRXCK=footrack"));
    OmahaRequestParams out;
    EXPECT_TRUE(DoTest(&out));
    EXPECT_TRUE(IsValidGuid(out.machine_id));
    // for now we're just using the machine id here
    EXPECT_TRUE(IsValidGuid(out.user_id));
    EXPECT_EQ("Chrome OS", out.os_platform);
    EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp);
    EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id);
    EXPECT_EQ("0.2.2.3", out.app_version);
    EXPECT_EQ("en-US", out.app_lang);
    EXPECT_EQ("", out.app_track);
  }
  EXPECT_EQ(0, System(string("rm -rf ") + kTestDir));
}

TEST_F(OmahaRequestDeviceParamsTest, ConfusingReleaseTest) {
  ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir + "/etc"));
  ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir +
                      utils::kStatefulPartition + "/etc"));
  {
    ASSERT_TRUE(WriteFileString(
        kTestDir + "/etc/lsb-release",
        "CHROMEOS_RELEASE_FOO=CHROMEOS_RELEASE_VERSION=1.2.3.4\n"
        "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
        "CHROMEOS_RELEASE_TRXCK=footrack"));
    OmahaRequestParams out;
    EXPECT_TRUE(DoTest(&out));
    EXPECT_TRUE(IsValidGuid(out.machine_id)) << out.machine_id;
    // for now we're just using the machine id here
    EXPECT_TRUE(IsValidGuid(out.user_id));
    EXPECT_EQ("Chrome OS", out.os_platform);
    EXPECT_EQ(string("0.2.2.3_") + GetMachineType(), out.os_sp);
    EXPECT_EQ("{87efface-864d-49a5-9bb3-4b050a7c227a}", out.app_id);
    EXPECT_EQ("0.2.2.3", out.app_version);
    EXPECT_EQ("en-US", out.app_lang);
    EXPECT_EQ("", out.app_track);
  }
  EXPECT_EQ(0, System(string("rm -rf ") + kTestDir));
}

TEST_F(OmahaRequestDeviceParamsTest, MachineIdPersistsTest) {
  ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir + "/etc"));
  ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir +
                      utils::kStatefulPartition + "/etc"));
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_FOO=CHROMEOS_RELEASE_VERSION=1.2.3.4\n"
      "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
      "CHROMEOS_RELEASE_TRXCK=footrack"));
  OmahaRequestParams out1;
  EXPECT_TRUE(DoTest(&out1));
  string machine_id;
  EXPECT_TRUE(utils::ReadFileToString(
      kTestDir +
      utils::kStatefulPartition + "/etc/omaha_id",
      &machine_id));
  EXPECT_EQ(machine_id, out1.machine_id);
  OmahaRequestParams out2;
  EXPECT_TRUE(DoTest(&out2));
  EXPECT_EQ(machine_id, out2.machine_id);
  EXPECT_EQ(0, System(string("rm -rf ") + kTestDir));
}

TEST_F(OmahaRequestDeviceParamsTest, NoDeltasTest) {
  ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir + "/etc"));
  ASSERT_EQ(0, System(string("mkdir -p ") + kTestDir +
                      utils::kStatefulPartition + "/etc"));
  ASSERT_TRUE(WriteFileString(
      kTestDir + "/etc/lsb-release",
      "CHROMEOS_RELEASE_FOO=CHROMEOS_RELEASE_VERSION=1.2.3.4\n"
      "CHROMEOS_RELEASE_VERSION=0.2.2.3\n"
      "CHROMEOS_RELEASE_TRXCK=footrack"));
  ASSERT_TRUE(WriteFileString(kTestDir + "/.nodelta", ""));
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out));
  EXPECT_FALSE(out.delta_okay);
  EXPECT_EQ(0, System(string("rm -rf ") + kTestDir));
}

}  // namespace chromeos_update_engine
