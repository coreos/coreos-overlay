// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <set>
#include <string>
#include <vector>

#include <gflags/gflags.h>
#include <glib.h>
#include "chromeos/obsolete_logging.h"

#include "update_engine/delta_diff_generator.h"
#include "update_engine/delta_diff_parser.h"
#include "update_engine/filesystem_copier_action.h"
#include "update_engine/install_action.h"
#include "update_engine/install_plan.h"
#include "update_engine/test_utils.h"
#include "update_engine/update_metadata.pb.h"
#include "update_engine/utils.h"

// This file contains a simple program that unpacks a delta diff into a
// directory.

using std::set;
using std::string;
using std::vector;
using std::tr1::shared_ptr;

DEFINE_string(delta, "", "the delta file");
DEFINE_string(output_dev, "", "the output device");
DEFINE_string(root, "", "the fake system root");
DEFINE_string(dump_delta, "", "path to delta file to dump");

namespace chromeos_update_engine {

class TestInstaller : public ActionProcessorDelegate {
 public:
  TestInstaller(GMainLoop *loop,
                const string& sys_root,
                const string& delta_path,
                const string& install_path)
      : loop_(loop),
        sys_root_(sys_root),
        delta_path_(delta_path),
        install_path_(install_path) {}
  void Update();

  // Delegate method:
  void ProcessingDone(const ActionProcessor* processor, bool success);
 private:
  vector<shared_ptr<AbstractAction> > actions_;
  ActionProcessor processor_;
  GMainLoop *loop_;
  string sys_root_;
  string delta_path_;
  string install_path_;
};

// Returns true on success. If there was no update available, that's still
// success.
// If force_full is true, try to force a full update.
void TestInstaller::Update() {
  CHECK(!processor_.IsRunning());
  processor_.set_delegate(this);

  // Actions:
  shared_ptr<ObjectFeederAction<InstallPlan> > object_feeder_action(
      new ObjectFeederAction<InstallPlan>);
  shared_ptr<FilesystemCopierAction> filesystem_copier_action(
      new FilesystemCopierAction);
  shared_ptr<InstallAction> install_action(
      new InstallAction);

  actions_.push_back(object_feeder_action);
  actions_.push_back(filesystem_copier_action);
  actions_.push_back(install_action);

  // Enqueue the actions
  for (vector<shared_ptr<AbstractAction> >::iterator it = actions_.begin();
       it != actions_.end(); ++it) {
    processor_.EnqueueAction(it->get());
  }

  // Bond them together. We have to use the leaf-types when calling
  // BondActions().
  BondActions(object_feeder_action.get(), filesystem_copier_action.get());
  BondActions(filesystem_copier_action.get(), install_action.get());
  
  InstallPlan install_plan(false,
                           "",
                           "",
                           delta_path_,
                           install_path_);
  filesystem_copier_action->set_copy_source(sys_root_);
  object_feeder_action->set_obj(install_plan);
  processor_.StartProcessing();
}

void TestInstaller::ProcessingDone(const ActionProcessor* processor,
                                   bool success) {
  LOG(INFO) << "install " << (success ? "succeeded" : "failed");
  actions_.clear();
  g_main_loop_quit(loop_);
}

gboolean TestInstallInMainLoop(void* arg) {
  TestInstaller* test_installer = reinterpret_cast<TestInstaller*>(arg);
  test_installer->Update();
  return FALSE;  // Don't call this callback function again
}


void usage(const char* argv0) {
  printf("usage: %s --root=system_root --delta=delta_file "
         "--output_dev=output_dev\n", argv0);
  exit(1);
}

bool Main(int argc, char** argv) {
  g_thread_init(NULL);
  google::ParseCommandLineFlags(&argc, &argv, true);
  logging::InitLogging("",
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);

  LOG(INFO) << "starting";
  
  if (!FLAGS_dump_delta.empty()) {
    CHECK(FLAGS_delta.empty());
    CHECK(FLAGS_root.empty());
    CHECK(FLAGS_output_dev.empty());
    
    DeltaDiffParser parser(FLAGS_dump_delta);
    for (DeltaDiffParser::Iterator it = parser.Begin();
         it != parser.End(); it.Increment()) {
      DeltaArchiveManifest_File file = it.GetFile();
      const char* format = "---";
      ssize_t offset = -1;
      ssize_t length = -1;
      if (file.has_data_format()) {
        switch (file.data_format()) {
          case DeltaArchiveManifest_File_DataFormat_FULL:
          format = "FULL";
          break;
          case DeltaArchiveManifest_File_DataFormat_FULL_GZ:
          format = "FULL_GZ";
          break;
          case DeltaArchiveManifest_File_DataFormat_BSDIFF:
          format = "BSDIFF";
          break;
          case DeltaArchiveManifest_File_DataFormat_COURGETTE:
          format = "COURGETTE";
          break;
          default:
            format = "???";
        }
        if (file.has_data_offset())
          offset = file.data_offset();
        if (file.has_data_length())
          length = file.data_length();
      }
      printf("%s %o %s %d %d\n", it.path().c_str(), file.mode(), format, offset,
             length);
    }
    exit(0);
  }

  CHECK(!FLAGS_delta.empty());
  CHECK(!FLAGS_root.empty());
  CHECK(!FLAGS_output_dev.empty());

  // Create the single GMainLoop
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);

  chromeos_update_engine::TestInstaller test_installer(loop,
                                                       FLAGS_root,
                                                       FLAGS_delta,
                                                       FLAGS_output_dev);

  g_timeout_add(0, &chromeos_update_engine::TestInstallInMainLoop,
                &test_installer);

  g_main_loop_run(loop);
  g_main_loop_unref(loop);

  LOG(INFO) << "Done.";
  return true;
}

}  // namespace chromeos_update_engine

int main(int argc, char** argv) {
  return chromeos_update_engine::Main(argc, argv) ? 0 : 1;
}
