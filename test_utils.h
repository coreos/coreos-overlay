// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_TEST_UTILS_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_TEST_UTILS_H__

#include <set>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "update_engine/action.h"

// These are some handy functions for unittests.

namespace chromeos_update_engine {

// Writes the data passed to path. The file at path will be overwritten if it
// exists. Returns true on success, false otherwise.
bool WriteFileVector(const std::string& path, const std::vector<char>& data);
bool WriteFileString(const std::string& path, const std::string& data);

// Returns the size of the file at path. If the file doesn't exist or some
// error occurrs, -1 is returned.
off_t FileSize(const std::string& path);

// Reads a symlink from disk. Returns empty string on failure.
std::string Readlink(const std::string& path);

// Gzip compresses the data passed using the gzip command line program.
// Returns compressed data back.
std::vector<char> GzipCompressData(const std::vector<char>& data);

// Gives back a 512-bytes length array that contains an MBR with
// the first partition is marked bootable.
std::vector<char> GenerateSampleMbr();

std::string GetUnusedLoopDevice();

// Returns true iff a == b
bool ExpectVectorsEq(const std::vector<char>& a, const std::vector<char>& b);

inline int System(const std::string& cmd) {
  return system(cmd.c_str());
}

void FillWithData(std::vector<char>* buffer);

namespace {
// 300 byte pseudo-random string. Not null terminated.
// This does not gzip compress well.
const unsigned char kRandomString[] = {
  0xf2, 0xb7, 0x55, 0x92, 0xea, 0xa6, 0xc9, 0x57,
  0xe0, 0xf8, 0xeb, 0x34, 0x93, 0xd9, 0xc4, 0x8f,
  0xcb, 0x20, 0xfa, 0x37, 0x4b, 0x40, 0xcf, 0xdc,
  0xa5, 0x08, 0x70, 0x89, 0x79, 0x35, 0xe2, 0x3d,
  0x56, 0xa4, 0x75, 0x73, 0xa3, 0x6d, 0xd1, 0xd5,
  0x26, 0xbb, 0x9c, 0x60, 0xbd, 0x2f, 0x5a, 0xfa,
  0xb7, 0xd4, 0x3a, 0x50, 0xa7, 0x6b, 0x3e, 0xfd,
  0x61, 0x2b, 0x3a, 0x31, 0x30, 0x13, 0x33, 0x53,
  0xdb, 0xd0, 0x32, 0x71, 0x5c, 0x39, 0xed, 0xda,
  0xb4, 0x84, 0xca, 0xbc, 0xbd, 0x78, 0x1c, 0x0c,
  0xd8, 0x0b, 0x41, 0xe8, 0xe1, 0xe0, 0x41, 0xad,
  0x03, 0x12, 0xd3, 0x3d, 0xb8, 0x75, 0x9b, 0xe6,
  0xd9, 0x01, 0xd0, 0x87, 0xf4, 0x36, 0xfa, 0xa7,
  0x0a, 0xfa, 0xc5, 0x87, 0x65, 0xab, 0x9a, 0x7b,
  0xeb, 0x58, 0x23, 0xf0, 0xa8, 0x0a, 0xf2, 0x33,
  0x3a, 0xe2, 0xe3, 0x35, 0x74, 0x95, 0xdd, 0x3c,
  0x59, 0x5a, 0xd9, 0x52, 0x3a, 0x3c, 0xac, 0xe5,
  0x15, 0x87, 0x6d, 0x82, 0xbc, 0xf8, 0x7d, 0xbe,
  0xca, 0xd3, 0x2c, 0xd6, 0xec, 0x38, 0xeb, 0xe4,
  0x53, 0xb0, 0x4c, 0x3f, 0x39, 0x29, 0xf7, 0xa4,
  0x73, 0xa8, 0xcb, 0x32, 0x50, 0x05, 0x8c, 0x1c,
  0x1c, 0xca, 0xc9, 0x76, 0x0b, 0x8f, 0x6b, 0x57,
  0x1f, 0x24, 0x2b, 0xba, 0x82, 0xba, 0xed, 0x58,
  0xd8, 0xbf, 0xec, 0x06, 0x64, 0x52, 0x6a, 0x3f,
  0xe4, 0xad, 0xce, 0x84, 0xb4, 0x27, 0x55, 0x14,
  0xe3, 0x75, 0x59, 0x73, 0x71, 0x51, 0xea, 0xe8,
  0xcc, 0xda, 0x4f, 0x09, 0xaf, 0xa4, 0xbc, 0x0e,
  0xa6, 0x1f, 0xe2, 0x3a, 0xf8, 0x96, 0x7d, 0x30,
  0x23, 0xc5, 0x12, 0xb5, 0xd8, 0x73, 0x6b, 0x71,
  0xab, 0xf1, 0xd7, 0x43, 0x58, 0xa7, 0xc9, 0xf0,
  0xe4, 0x85, 0x1c, 0xd6, 0x92, 0x50, 0x2c, 0x98,
  0x36, 0xfe, 0x87, 0xaf, 0x43, 0x8f, 0x8f, 0xf5,
  0x88, 0x48, 0x18, 0x42, 0xcf, 0x42, 0xc1, 0xa8,
  0xe8, 0x05, 0x08, 0xa1, 0x45, 0x70, 0x5b, 0x8c,
  0x39, 0x28, 0xab, 0xe9, 0x6b, 0x51, 0xd2, 0xcb,
  0x30, 0x04, 0xea, 0x7d, 0x2f, 0x6e, 0x6c, 0x3b,
  0x5f, 0x82, 0xd9, 0x5b, 0x89, 0x37, 0x65, 0x65,
  0xbe, 0x9f, 0xa3, 0x5d
};

const char* const kMountPath = "/tmp/UpdateEngineTests_mnt";
}  // namespace {}

// Creates an ext image with some files in it. The paths creates are
// returned in out_paths.
void CreateExtImageAtPath(const std::string& path,
                          std::vector<std::string>* out_paths);

// Verifies that for each path in paths, it exists in the filesystem under
// parent. Also, verifies that no additional paths are present under parent.
// Also tests properties of various files created by CreateExtImageAtPath().
// Intentionally copies expected_paths.
void VerifyAllPaths(const std::string& parent,
                    std::set<std::string> expected_paths);

// Useful actions for test

class NoneType;

template<typename T>
class ObjectFeederAction;

template<typename T>
class ActionTraits<ObjectFeederAction<T> > {
 public:
  typedef T OutputObjectType;
  typedef NoneType InputObjectType;
};

// This is a simple Action class for testing. It feeds an object into
// another action.
template<typename T>
struct ObjectFeederAction : public Action<ObjectFeederAction<T> > {
 public:
  typedef NoneType InputObjectType;
  typedef T OutputObjectType;
  void PerformAction() {
    LOG(INFO) << "feeder running!";
    CHECK(this->processor_);
    if (this->HasOutputPipe()) {
      this->SetOutputObject(out_obj_);
    }
    this->processor_->ActionComplete(this, true);
  }
  static std::string StaticType() { return "ObjectFeederAction"; }
  std::string Type() const { return StaticType(); }
  void set_obj(const T& out_obj) {
    out_obj_ = out_obj;
  }
 private:
  T out_obj_;
};

template<typename T>
class ObjectCollectorAction;

template<typename T>
class ActionTraits<ObjectCollectorAction<T> > {
 public:
  typedef NoneType OutputObjectType;
  typedef T InputObjectType;
};

// This is a simple Action class for testing. It receives an object from
// another action.
template<typename T>
struct ObjectCollectorAction : public Action<ObjectCollectorAction<T> > {
 public:
  typedef T InputObjectType;
  typedef NoneType OutputObjectType;
  void PerformAction() {
    LOG(INFO) << "collector running!";
    ASSERT_TRUE(this->processor_);
    if (this->HasInputObject()) {
      object_ = this->GetInputObject();
    }
    this->processor_->ActionComplete(this, true);
  }
  static std::string StaticType() { return "ObjectCollectorAction"; }
  std::string Type() const { return StaticType(); }
  const T& object() const { return object_; }
 private:
  T object_;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_TEST_UTILS_H__