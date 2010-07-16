// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <glib.h>

#include "update_engine/marshal.glibmarshal.h"
#include "update_engine/dbus_constants.h"
#include "update_engine/subprocess.h"
#include "update_engine/utils.h"

extern "C" {
#include "update_engine/update_engine.dbusclient.h"
}

using chromeos_update_engine::kUpdateEngineServiceName;
using chromeos_update_engine::kUpdateEngineServicePath;
using chromeos_update_engine::kUpdateEngineServiceInterface;
using chromeos_update_engine::utils::GetGErrorMessage;

DEFINE_bool(status, false, "Print the status to stdout.");
DEFINE_bool(force_update, false,
            "Force an update, even over an expensive network.");
DEFINE_bool(check_for_update, false,
            "Initiate check for updates.");
DEFINE_bool(watch_for_updates, false,
            "Listen for status updates and print them to the screen.");

namespace {

bool GetProxy(DBusGProxy** out_proxy) {
  DBusGConnection* bus;
  DBusGProxy* proxy;
  GError* error = NULL;

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
    LOG(FATAL) << "Error getting proxy: " << GetGErrorMessage(error);
  }
  *out_proxy = proxy;
  return true;
}

static void StatusUpdateSignalHandler(DBusGProxy* proxy,
                                      int64_t last_checked_time,
                                      double progress,
                                      gchar* current_operation,
                                      gchar* new_version,
                                      int64_t new_size,
                                      void* user_data) {
  LOG(INFO) << "Got status update:";
  LOG(INFO) << "  last_checked_time: " << last_checked_time;
  LOG(INFO) << "  progress: " << progress;
  LOG(INFO) << "  current_operation: " << current_operation;
  LOG(INFO) << "  new_version: " << new_version;
  LOG(INFO) << "  new_size: " << new_size;
}

bool GetStatus() {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

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
    LOG(INFO) << "Error getting status: " << GetGErrorMessage(error);
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

// Should never return.
void WatchForUpdates() {
  DBusGProxy* proxy;

  CHECK(GetProxy(&proxy));

  // Register marshaller
  dbus_g_object_register_marshaller(
      update_engine_VOID__INT64_DOUBLE_STRING_STRING_INT64,
      G_TYPE_NONE,
      G_TYPE_INT64,
      G_TYPE_DOUBLE,
      G_TYPE_STRING,
      G_TYPE_STRING,
      G_TYPE_INT64,
      G_TYPE_INVALID);

  static const char kStatusUpdate[] = "StatusUpdate";
  dbus_g_proxy_add_signal(proxy,
                          kStatusUpdate,
                          G_TYPE_INT64,
                          G_TYPE_DOUBLE,
                          G_TYPE_STRING,
                          G_TYPE_STRING,
                          G_TYPE_INT64,
                          G_TYPE_INVALID);
  GMainLoop* loop = g_main_loop_new (NULL, TRUE);
  dbus_g_proxy_connect_signal(proxy,
                              kStatusUpdate,
                              G_CALLBACK(StatusUpdateSignalHandler),
                              NULL,
                              NULL);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
}

bool CheckForUpdates(bool force) {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  gboolean rc =
      org_chromium_UpdateEngineInterface_check_for_update(proxy, &error);
  CHECK_EQ(rc, TRUE) << "Error checking for update: "
                     << GetGErrorMessage(error);
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
  if (FLAGS_watch_for_updates) {
    LOG(INFO) << "Watching for status updates.";
    WatchForUpdates();  // Should never return.
    return 1;
  }

  LOG(INFO) << "No flags specified. Exiting.";
  return 0;
}
