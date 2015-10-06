// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <dbus/dbus.h>
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
using chromeos_update_engine::utils::GetAndFreeGError;
using std::string;

DEFINE_bool(check_for_update, false, "Initiate check for updates.");
DEFINE_bool(status, false, "Print the status to stdout.");
DEFINE_bool(reset_status, false, "Sets the status in update_engine to idle.");
DEFINE_bool(update, false, "Forces an update and waits for its completion. "
            "Exit status is 0 if the update succeeded, and 1 otherwise.");
DEFINE_bool(watch_for_updates, false,
            "Listen for status updates and print them to the screen.");

namespace {

bool GetProxy(DBusGProxy** out_proxy) {
  DBusGConnection* bus;
  DBusGProxy* proxy = NULL;
  GError* error = NULL;
  const int kTries = 4;
  const int kRetrySeconds = 10;

  bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
  if (bus == NULL) {
    LOG(ERROR) << "Failed to get bus: " << GetAndFreeGError(&error);
    exit(1);
  }
  for (int i = 0; !proxy && i < kTries; ++i) {
    if (i > 0) {
      LOG(INFO) << "Retrying to get dbus proxy. Try "
                << (i + 1) << "/" << kTries;
      g_usleep(kRetrySeconds * G_USEC_PER_SEC);
    }
    proxy = dbus_g_proxy_new_for_name_owner(bus,
                                            kUpdateEngineServiceName,
                                            kUpdateEngineServicePath,
                                            kUpdateEngineServiceInterface,
                                            &error);
    LOG_IF(WARNING, !proxy) << "Error getting dbus proxy for "
                            << kUpdateEngineServiceName << ": "
                            << GetAndFreeGError(&error);
  }
  if (proxy == NULL) {
    LOG(ERROR) << "Giving up -- unable to get dbus proxy for "
               << kUpdateEngineServiceName;
    exit(1);
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

bool ResetStatus() {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  gboolean rc =
      com_coreos_update1_Manager_reset_status(proxy, &error);
  return rc;
}


// If |op| is non-NULL, sets it to the current operation string or an
// empty string if unable to obtain the current status.
bool GetStatus(string* op) {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  gint64 last_checked_time = 0;
  gdouble progress = 0.0;
  char* current_op = NULL;
  char* new_version = NULL;
  gint64 new_size = 0;

  gboolean rc = com_coreos_update1_Manager_get_status(
      proxy,
      &last_checked_time,
      &progress,
      &current_op,
      &new_version,
      &new_size,
      &error);
  if (rc == FALSE) {
    LOG(INFO) << "Error getting status: " << GetAndFreeGError(&error);
  }
  printf("LAST_CHECKED_TIME=%" PRIi64 "\nPROGRESS=%f\nCURRENT_OP=%s\n"
         "NEW_VERSION=%s\nNEW_SIZE=%" PRIi64 "\n",
         last_checked_time,
         progress,
         current_op,
         new_version,
         new_size);
  if (op) {
    *op = current_op ? current_op : "";
  }
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

bool CheckForUpdates() {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  gboolean rc =
      com_coreos_update1_Manager_attempt_update(proxy, &error);
  CHECK_EQ(rc, TRUE) << "Error checking for update: "
                     << GetAndFreeGError(&error);
  return true;
}

static gboolean CompleteUpdateSource(gpointer data) {
  string current_op;
  if (!GetStatus(&current_op) || current_op == "UPDATE_STATUS_IDLE") {
    LOG(ERROR) << "Update failed.";
    exit(1);
  }
  if (current_op == "UPDATE_STATUS_UPDATED_NEED_REBOOT") {
    LOG(INFO) << "Update succeeded -- reboot needed.";
    exit(0);
  }
  return TRUE;
}

// This is similar to watching for updates but rather than registering
// a signal watch, activelly poll the daemon just in case it stops
// sending notifications.
void CompleteUpdate() {
  GMainLoop* loop = g_main_loop_new (NULL, TRUE);
  g_timeout_add_seconds(5, CompleteUpdateSource, NULL);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
}

}  // namespace {}

int main(int argc, char** argv) {
  // Boilerplate init commands.
  dbus_threads_init_default();
  chromeos_update_engine::Subprocess::Init();
  GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);

  // Update the status if requested.
  if (FLAGS_reset_status) {
    LOG(INFO) << "Setting Update Engine status to idle ...";
    if (!ResetStatus()) {
      LOG(ERROR) << "ResetStatus failed.";
      return 1;
    }

    LOG(INFO) << "ResetStatus succeeded; to undo partition table changes run:\n"
                 "(D=$(rootdev -d) P=$(rootdev -s); cgpt p -i$(($(echo ${P#$D} "
                 "| sed 's/^[^0-9]*//')-1)) $D;)";
    return 0;
  }


  if (FLAGS_status) {
    LOG(INFO) << "Querying Update Engine status...";
    if (!GetStatus(NULL)) {
      LOG(FATAL) << "GetStatus failed.";
      return 1;
    }
    return 0;
  }

  // Initiate an update check, if necessary.
  if (FLAGS_check_for_update || FLAGS_update) {
    LOG(INFO) << "Initiating update check and install.";
    CHECK(CheckForUpdates()) << "Update check/initiate update failed.";

    // Wait for an update to complete.
    if (FLAGS_update) {
      LOG(INFO) << "Waiting for update to complete.";
      CompleteUpdate();  // Should never return.
      return 1;
    }
    return 0;
  }

  // Start watching for updates.
  if (FLAGS_watch_for_updates) {
    LOG(INFO) << "Watching for status updates.";
    WatchForUpdates();  // Should never return.
    return 1;
  }

  LOG(INFO) << "Done.";
  return 0;
}
