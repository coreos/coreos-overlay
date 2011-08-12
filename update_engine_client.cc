// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

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
using std::string;

DEFINE_string(app_version, "", "Force the current app version.");
DEFINE_bool(check_for_update, false, "Initiate check for updates.");
DEFINE_string(omaha_url, "", "The URL of the Omaha update server.");
DEFINE_bool(reboot, false, "Initiate a reboot if needed.");
DEFINE_bool(show_track, false, "Show the update track.");
DEFINE_bool(status, false, "Print the status to stdout.");
DEFINE_string(track, "", "Permanently change the update track.");
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

  bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
  if (!bus) {
    LOG(FATAL) << "Failed to get bus";
  }
  for (int i = 0; !proxy && i < kTries; ++i) {
    LOG_IF(INFO, i) << "Trying to get dbus proxy. Try "
                    << (i + 1) << "/" << kTries;
    proxy = dbus_g_proxy_new_for_name_owner(bus,
                                            kUpdateEngineServiceName,
                                            kUpdateEngineServicePath,
                                            kUpdateEngineServiceInterface,
                                            &error);
  }
  if (!proxy) {
    LOG(FATAL) << "Error getting dbus proxy for "
               << kUpdateEngineServiceName << ": " << GetGErrorMessage(error);
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

bool CheckForUpdates(const string& app_version, const string& omaha_url) {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  gboolean rc =
      org_chromium_UpdateEngineInterface_attempt_update(proxy,
                                                        app_version.c_str(),
                                                        omaha_url.c_str(),
                                                        &error);
  CHECK_EQ(rc, TRUE) << "Error checking for update: "
                     << GetGErrorMessage(error);
  return true;
}

bool RebootIfNeeded() {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  gboolean rc =
      org_chromium_UpdateEngineInterface_reboot_if_needed(proxy, &error);
  // Reboot error code doesn't necessarily mean that a reboot
  // failed. For example, D-Bus may be shutdown before we receive the
  // result.
  LOG_IF(INFO, !rc) << "Reboot error message: " << GetGErrorMessage(error);
  return true;
}

void SetTrack(const string& track) {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  gboolean rc =
      org_chromium_UpdateEngineInterface_set_track(proxy,
                                                   track.c_str(),
                                                   &error);
  CHECK_EQ(rc, true) << "Error setting the track: "
                     << GetGErrorMessage(error);
  LOG(INFO) << "Track permanently set to: " << track;
}

string GetTrack() {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  char* track = NULL;
  gboolean rc =
      org_chromium_UpdateEngineInterface_get_track(proxy,
                                                   &track,
                                                   &error);
  CHECK_EQ(rc, true) << "Error getting the track: "
                     << GetGErrorMessage(error);
  string output = track;
  g_free(track);
  return output;
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
  g_type_init();
  g_thread_init(NULL);
  dbus_g_thread_init();
  chromeos_update_engine::Subprocess::Init();
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_status) {
    LOG(INFO) << "Querying Update Engine status...";
    if (!GetStatus(NULL)) {
      LOG(FATAL) << "GetStatus failed.";
      return 1;
    }
    return 0;
  }

  // First, update the track if requested.
  if (!FLAGS_track.empty()) {
    SetTrack(FLAGS_track);
  }

  // Show the track if requested.
  if (FLAGS_show_track) {
    LOG(INFO) << "Track: " << GetTrack();
  }

  // Initiate an update check, if necessary.
  if (FLAGS_check_for_update ||
      FLAGS_update ||
      !FLAGS_app_version.empty() ||
      !FLAGS_omaha_url.empty()) {
    LOG_IF(WARNING, FLAGS_reboot) << "-reboot flag ignored.";
    string app_version = FLAGS_app_version;
    if (FLAGS_update && app_version.empty()) {
      app_version = "ForcedUpdate";
      LOG(INFO) << "Forcing an update by setting app_version to ForcedUpdate.";
    }
    LOG(INFO) << "Initiating update check and install.";
    CHECK(CheckForUpdates(app_version, FLAGS_omaha_url))
        << "Update check/initiate update failed.";

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
    LOG_IF(WARNING, FLAGS_reboot) << "-reboot flag ignored.";
    LOG(INFO) << "Watching for status updates.";
    WatchForUpdates();  // Should never return.
    return 1;
  }

  if (FLAGS_reboot) {
    LOG(INFO) << "Requesting a reboot...";
    CHECK(RebootIfNeeded());
    return 0;
  }

  LOG(INFO) << "Done.";
  return 0;
}
