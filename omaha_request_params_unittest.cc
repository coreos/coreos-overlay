// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
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
  }

  virtual void TearDown() {
    EXPECT_EQ(0, System(string("rm -rf ") + kTestDir));
  }

  static const string kTestDir;
};

const string OmahaRequestDeviceParamsTest::kTestDir =
    "omaha_request_device_params-test";

bool OmahaRequestDeviceParamsTest::DoTest(OmahaRequestParams* out,
                                          const string& app_version,
                                          const string& omaha_url) {
  OmahaRequestDeviceParams params;
  params.set_root(string("./") + kTestDir);
  bool success = params.Init(app_version, omaha_url);
  if (out)
    *out = params;
  return success;
}

namespace {
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

TEST_F(OmahaRequestDeviceParamsTest, HardwareClassTest) {
  string test_class = " \t sample hardware class \n ";
  FilePath hwid_path(kTestDir + "/sys/devices/platform/chromeos_acpi/HWID");
  ASSERT_TRUE(file_util::CreateDirectory(hwid_path.DirName()));
  ASSERT_EQ(test_class.size(),
            file_util::WriteFile(hwid_path,
                                 test_class.data(),
                                 test_class.size()));
  OmahaRequestParams out;
  EXPECT_TRUE(DoTest(&out, "", ""));
  EXPECT_EQ("sample hardware class", out.hardware_class);
}

}  // namespace chromeos_update_engine
