// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_INSTALL_ACTION_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_INSTALL_ACTION_H__

#include "base/scoped_ptr.h"
#include "update_engine/action.h"
#include "update_engine/delta_diff_parser.h"
#include "update_engine/install_plan.h"
#include "update_engine/update_metadata.pb.h"

// The Install Action is responsible for ensuring the update that's been
// downloaded has been installed. This may be a no-op in the case of a full
// update, since those will be downloaded directly into the destination
// partition. However, for a delta update some work is required.

// An InstallPlan struct must be passed to this action before PerformAction()
// is called so that this action knows if it's a delta update, and if so,
// what the paths are.

// TODO(adlr): At the moment, InstallAction is synchronous. It should be
// updated to be asynchronous at some point.

namespace chromeos_update_engine {

class InstallAction;
class NoneType;

template<>
class ActionTraits<InstallAction> {
 public:
  // Takes the InstallPlan for input
  typedef InstallPlan InputObjectType;
  // On success, puts the output device path on output
  typedef std::string OutputObjectType;
};

class InstallAction : public Action<InstallAction> {
 public:
  InstallAction() {}
  typedef ActionTraits<InstallAction>::InputObjectType InputObjectType;
  typedef ActionTraits<InstallAction>::OutputObjectType OutputObjectType;
  void PerformAction();

  // This action is synchronous for now.
  void TerminateProcessing() { CHECK(false); }

  // Debugging/logging
  static std::string StaticType() { return "InstallAction"; }
  std::string Type() const { return StaticType(); }

 private:
  // Installs 'file' into mountpoint. 'path' is the path that 'file'
  // should have when we reboot and mountpoint is root.
  bool InstallFile(const std::string& mountpoint,
                   const DeltaArchiveManifest_File& file,
                   const std::string& path,
                   const DeltaDiffParser& parser) const;
  // These are helpers for InstallFile. They focus on specific file types:
  // Regular data files:
  bool InstallFileRegularFile(const std::string& mountpoint,
                              const DeltaArchiveManifest_File& file,
                              const std::string& path,
                              const DeltaDiffParser& parser,
                              const bool exists) const;
  // char/block devices, fifos, and sockets:
  bool InstallFileSpecialFile(const std::string& mountpoint,
                              const DeltaArchiveManifest_File& file,
                              const std::string& path,
                              const DeltaDiffParser& parser,
                              const bool exists) const;
  // symlinks:
  bool InstallFileSymlink(const std::string& mountpoint,
                          const DeltaArchiveManifest_File& file,
                          const std::string& path,
                          const DeltaDiffParser& parser,
                          const bool exists) const;

  DISALLOW_COPY_AND_ASSIGN(InstallAction);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_INSTALL_ACTION_H__
