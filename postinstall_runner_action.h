// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POSTINSTALL_RUNNER_ACTION_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POSTINSTALL_RUNNER_ACTION_H__

#include <string>
#include "update_engine/action.h"

// The Postinstall Runner Action is responsible for running the postinstall
// script of a successfully downloaded update.

namespace chromeos_update_engine {

class PostinstallRunnerAction;
class NoneType;

template<>
class ActionTraits<PostinstallRunnerAction> {
 public:
  // Takes the device path as input
  typedef std::string InputObjectType;
  // Passes the device path as output
  typedef std::string OutputObjectType;
};

class PostinstallRunnerAction : public Action<PostinstallRunnerAction> {
 public:
  PostinstallRunnerAction() {}
  typedef ActionTraits<PostinstallRunnerAction>::InputObjectType
      InputObjectType;
  typedef ActionTraits<PostinstallRunnerAction>::OutputObjectType
      OutputObjectType;
  void PerformAction();

  // This is a synchronous action, and thus TerminateProcessing() should
  // never be called
  void TerminateProcessing() { CHECK(false); }

  // Debugging/logging
  static std::string StaticType() { return "PostinstallRunnerAction"; }
  std::string Type() const { return StaticType(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(PostinstallRunnerAction);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POSTINSTALL_RUNNER_ACTION_H__
