// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_POSTINSTALL_RUNNER_ACTION_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_POSTINSTALL_RUNNER_ACTION_H__

#include <string>

#include "update_engine/action.h"
#include "update_engine/install_plan.h"

// The Postinstall Runner Action is responsible for running the postinstall
// script of a successfully downloaded update.

namespace chromeos_update_engine {

class PostinstallRunnerAction;
class NoneType;

template<>
class ActionTraits<PostinstallRunnerAction> {
 public:
  // Takes the device path as input
  typedef InstallPlan InputObjectType;
  // Passes the device path as output
  typedef InstallPlan OutputObjectType;
};

class PostinstallRunnerAction : public Action<PostinstallRunnerAction> {
 public:
  PostinstallRunnerAction() {}
  typedef ActionTraits<PostinstallRunnerAction>::InputObjectType
      InputObjectType;
  typedef ActionTraits<PostinstallRunnerAction>::OutputObjectType
      OutputObjectType;
  void PerformAction();

  // Note that there's no support for terminating this action currently.
  void TerminateProcessing() { CHECK(false); }

  // Debugging/logging
  static std::string StaticType() { return "PostinstallRunnerAction"; }
  std::string Type() const { return StaticType(); }

 private:
  // Subprocess::Exec callback.
  void CompletePostinstall(int return_code);
  static void StaticCompletePostinstall(int return_code,
                                        const std::string& output,
                                        void* p);

  std::string temp_rootfs_dir_;

  DISALLOW_COPY_AND_ASSIGN(PostinstallRunnerAction);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_POSTINSTALL_RUNNER_ACTION_H__
