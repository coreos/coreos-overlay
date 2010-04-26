// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <glib.h>

#include "update_engine/dbus_constants.h"
#include "update_engine/subprocess.h"
#include "update_engine/utils.h"

extern "C" {
#include "update_engine/update_engine.dbusclient.h"
}

using chromeos_update_engine::kUpdateEngineServiceName;
using chromeos_update_engine::kUpdateEngineServicePath;
using chromeos_update_engine::kUpdateEngineServiceInterface;

DEFINE_bool(status, false, "Print the status to stdout.");
DEFINE_bool(force_update, false,
            "Force an update, even over an expensive network.");
DEFINE_bool(check_for_update, false,
            "Initiate check for updates.");

namespace {

const char* GetErrorMessage(const GError* error) {
  if (!error)
    return "Unknown error.";
  return error->message;
}

bool GetStatus() {
  DBusGConnection *bus;
  DBusGProxy *proxy;
  GError *error = NULL;

  bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
  if (!bus) {
    LOG(FATAL) << "Failed to get bus";
  }
  proxy = dbus_g_proxy_new_for_name_owner(bus,
                                          kUpdateEngineServiceName,
                                          kUpdateEngineServicePath,
                                          kUpdateEngineServiceInterface,
                                          &error);
  if (!proxy) {
    LOG(FATAL) << "Error getting proxy: " << GetErrorMessage(error);
  }

  gint64 last_checked_time = 0;
  gdouble progress = 0.0;
  char* current_op = NULL;
  char* new_version = NULL;
  gint64 new_size = 0;

  gboolean rc = org_chromium_UpdateEngineInterface_get_status(
      proxy,
      &last_checked_time,
      &progress,
      &current_op,
      &new_version,
      &new_size,
      &error);
  if (rc == FALSE) {
    LOG(INFO) << "Error getting status: " << GetErrorMessage(error);
  }
  printf("LAST_CHECKED_TIME=%" PRIi64 "\nPROGRESS=%f\nCURRENT_OP=%s\n"
         "NEW_VERSION=%s\nNEW_SIZE=%" PRIi64 "\n",
         last_checked_time,
         progress,
         current_op,
         new_version,
         new_size);
  return true;
}

bool CheckForUpdates(bool force) {
  return true;
}

}  // namespace {}

int main(int argc, char** argv) {
  // Boilerplate init commands.
  g_type_init();
  g_thread_init(NULL);
  dbus_g_thread_init();
  chromeos_update_engine::Subprocess::Init();
  google::ParseCommandLineFlags(&argc, &argv, true);
  
  if (FLAGS_status) {
    LOG(INFO) << "Querying Update Engine status...";
    if (!GetStatus()) {
      LOG(FATAL) << "GetStatus() failed.";
    }
    return 0;
  }
  if (FLAGS_force_update || FLAGS_check_for_update) {
    LOG(INFO) << "Initiating update check and install.";
    if (FLAGS_force_update) {
      LOG(INFO) << "Will not abort due to being on expensive network.";
    }
    CHECK(CheckForUpdates(FLAGS_force_update))
        << "Update check/initiate update failed.";
    return 0;
  }
  
  LOG(INFO) << "No flags specified. Exiting.";
  return 0;
}
