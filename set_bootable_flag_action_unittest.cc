// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "update_engine/set_bootable_flag_action.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

class SetBootableFlagActionProcessorDelegate : public ActionProcessorDelegate {
 public:
  SetBootableFlagActionProcessorDelegate()
      : success_(false), success_set_(false) {}
  void ActionCompleted(ActionProcessor* processor,
                       AbstractAction* action,
                       bool success) {
    if (action->Type() == SetBootableFlagAction::StaticType()) {
      success_ = success;
      success_set_ = true;
    }
  }
  bool success_;
  bool success_set_;
};

class SetBootableFlagActionTest : public ::testing::Test {
 public:
  SetBootableFlagActionTest();
 protected:
  enum TestFlags {
    EXPECT_SUCCESS = 0x01,
    WRITE_FILE = 0x02,
    SKIP_INPUT_OBJECT = 0x04
  };
  static const char* kTestDir;
  virtual void SetUp() {
    EXPECT_EQ(0, mkdir(kTestDir, 0755));
  }
  virtual void TearDown() {
    EXPECT_EQ(0, System(string("rm -rf ") + kTestDir));
  }
  vector<char> DoTest(vector<char> mbr_in,
                      const string& filename,
                      uint32 test_flags);
  // first partition bootable, no others bootable
  const vector<char> sample_mbr_;
};

const char* SetBootableFlagActionTest::kTestDir =
    "SetBootableFlagActionTestDir";

SetBootableFlagActionTest::SetBootableFlagActionTest()
    : sample_mbr_(GenerateSampleMbr()) {}

vector<char> SetBootableFlagActionTest::DoTest(vector<char> mbr_in,
                                               const string& filename,
                                               uint32 flags) {
  CHECK(!filename.empty());
  const string root_filename(filename.begin(), --filename.end());
  if (flags & WRITE_FILE)
    EXPECT_TRUE(WriteFileVector(root_filename, mbr_in));

  ActionProcessor processor;
  SetBootableFlagActionProcessorDelegate delegate;
  processor.set_delegate(&delegate);

  ObjectFeederAction<string> feeder_action;
  feeder_action.set_obj(filename);
  SetBootableFlagAction set_bootable_action;
  if (!(flags & SKIP_INPUT_OBJECT))
    BondActions(&feeder_action, &set_bootable_action);
  ObjectCollectorAction<string> collector_action;
  BondActions(&set_bootable_action, &collector_action);
  processor.EnqueueAction(&feeder_action);
  processor.EnqueueAction(&set_bootable_action);
  processor.EnqueueAction(&collector_action);
  processor.StartProcessing();
  EXPECT_TRUE(!processor.IsRunning())
      << "Update test to handle non-asynch actions";

  EXPECT_TRUE(delegate.success_set_);
  EXPECT_EQ(flags & EXPECT_SUCCESS, delegate.success_);

  vector<char> new_mbr;
  if (flags & WRITE_FILE)
    utils::ReadFile(root_filename, &new_mbr);

  unlink(root_filename.c_str());
  return new_mbr;
}

TEST_F(SetBootableFlagActionTest, SimpleTest) {
  for (int i = 0; i < 4; i++) {
    vector<char> expected_new_mbr = sample_mbr_;
    for (int j = 0; j < 4; j++)
      expected_new_mbr[446 + 16 * j] = '\0';  // mark non-bootable
    expected_new_mbr[446 + 16 * i] = 0x80;  // mark bootable

    string filename(string(kTestDir) + "/SetBootableFlagActionTest.devX");
    filename[filename.size() - 1] = '1' + i;

    vector<char> actual_new_mbr = DoTest(expected_new_mbr, filename,
                                         EXPECT_SUCCESS | WRITE_FILE);

    ExpectVectorsEq(expected_new_mbr, actual_new_mbr);
  }
}

TEST_F(SetBootableFlagActionTest, BadDeviceTest) {
  vector<char> actual_new_mbr = DoTest(sample_mbr_,
                                       string(kTestDir) +
                                       "SetBootableFlagActionTest.dev5",
                                       WRITE_FILE);
  ExpectVectorsEq(sample_mbr_, actual_new_mbr);  // no change

  actual_new_mbr = DoTest(sample_mbr_,
                          string(kTestDir) + "SetBootableFlagActionTest.dev13",
                          WRITE_FILE);
  ExpectVectorsEq(sample_mbr_, actual_new_mbr);  // no change

  actual_new_mbr = DoTest(sample_mbr_,
                          "/some/nonexistent/file3",
                          0);
  EXPECT_TRUE(actual_new_mbr.empty());

  vector<char> bad_mbr = sample_mbr_;
  bad_mbr[510] = 'x';  // makes signature invalid

  actual_new_mbr = DoTest(bad_mbr,
                          string(kTestDir) + "SetBootableFlagActionTest.dev2",
                          WRITE_FILE);
  ExpectVectorsEq(bad_mbr, actual_new_mbr);  // no change
}

TEST_F(SetBootableFlagActionTest, NoInputObjectTest) {
  vector<char> actual_new_mbr = DoTest(sample_mbr_,
                                       string(kTestDir) +
                                       "SetBootableFlagActionTest.dev5",
                                       WRITE_FILE | SKIP_INPUT_OBJECT);
  ExpectVectorsEq(sample_mbr_, actual_new_mbr);  // no change
}

}  // namespace chromeos_update_engine
