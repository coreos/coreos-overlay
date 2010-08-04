// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tr1/memory>
#include <vector>

#include <gflags/gflags.h>
#include <glib.h>

#include "base/command_line.h"
#include "chromeos/obsolete_logging.h"
#include "metrics/metrics_library.h"
#include "update_engine/dbus_constants.h"
#include "update_engine/dbus_service.h"
#include "update_engine/prefs.h"
#include "update_engine/update_attempter.h"
#include "update_engine/utils.h"

extern "C" {
#include "update_engine/update_engine.dbusserver.h"
}

DEFINE_bool(logtostderr, false,
            "Write logs to stderr instead of to a file in log_dir.");
DEFINE_bool(foreground, false,
            "Don't daemon()ize; run in foreground.");

using std::string;
using std::tr1::shared_ptr;
using std::vector;

namespace chromeos_update_engine {

namespace {

gboolean UpdateOnce(void* arg) {
  UpdateAttempter* update_attempter = reinterpret_cast<UpdateAttempter*>(arg);
  update_attempter->Update("", "");
  return FALSE;
}

gboolean UpdatePeriodically(void* arg) {
  UpdateAttempter* update_attempter = reinterpret_cast<UpdateAttempter*>(arg);
  update_attempter->Update("", "");
  return TRUE;
}

void SchedulePeriodicUpdateChecks(UpdateAttempter* update_attempter) {
  if (!utils::IsOfficialBuild()) {
    LOG(WARNING) << "No periodic update checks on non-official builds.";
    return;
  }

  // Kick off periodic updating. First, update after 2 minutes. Also, update
  // every 30 minutes.
  g_timeout_add(2 * 60 * 1000, &UpdateOnce, update_attempter);
  g_timeout_add(30 * 60 * 1000, &UpdatePeriodically, update_attempter);
}

void SetupDbusService(UpdateEngineService* service) {
  DBusGConnection *bus;
  DBusGProxy *proxy;
  GError *error = NULL;

  bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
  if (!bus) {
    LOG(FATAL) << "Failed to get bus";
  }
  proxy = dbus_g_proxy_new_for_name(bus,
                                    DBUS_SERVICE_DBUS,
                                    DBUS_PATH_DBUS,
                                    DBUS_INTERFACE_DBUS);

  guint32 request_name_ret;
  if (!org_freedesktop_DBus_request_name(proxy,
                                         kUpdateEngineServiceName,
                                         0,
                                         &request_name_ret,
                                         &error)) {
    LOG(FATAL) << "Failed to get name: " << error->message;
  }
  if (request_name_ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
    g_warning("Got result code %u from requesting name", request_name_ret);
    g_error_free(error);
    LOG(FATAL) << "Got result code " << request_name_ret
               << " from requesting name, but expected "
               << DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER;
  }
  dbus_g_connection_register_g_object(bus,
                                      "/org/chromium/UpdateEngine",
                                      G_OBJECT(service));
}

}  // namespace {}

}  // namespace chromeos_update_engine

#include "update_engine/subprocess.h"

int main(int argc, char** argv) {
  ::g_type_init();
  g_thread_init(NULL);
  dbus_g_thread_init();
  chromeos_update_engine::Subprocess::Init();
  google::ParseCommandLineFlags(&argc, &argv, true);
  CommandLine::Init(argc, argv);
  logging::InitLogging("/var/log/update_engine.log",
                       (FLAGS_logtostderr ?
                        logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG :
                        logging::LOG_ONLY_TO_FILE),
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
  if (!FLAGS_foreground)
    PLOG_IF(FATAL, daemon(0, 0) == 1) << "daemon() failed";

  LOG(INFO) << "Chrome OS Update Engine starting";

  // Create the single GMainLoop
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);

  chromeos_update_engine::Prefs prefs;
  LOG_IF(ERROR, !prefs.Init(FilePath("/var/lib/update_engine/prefs")))
      << "Failed to initialize preferences.";

  MetricsLibrary metrics_lib;
  metrics_lib.Init();

  // Create the update attempter:
  chromeos_update_engine::UpdateAttempter update_attempter(&prefs,
                                                           &metrics_lib);

  // Create the dbus service object:
  dbus_g_object_type_install_info(UPDATE_ENGINE_TYPE_SERVICE,
                                  &dbus_glib_update_engine_service_object_info);
  UpdateEngineService* service =
      UPDATE_ENGINE_SERVICE(g_object_new(UPDATE_ENGINE_TYPE_SERVICE, NULL));
  service->update_attempter_ = &update_attempter;
  update_attempter.set_dbus_service(service);
  chromeos_update_engine::SetupDbusService(service);

  chromeos_update_engine::SchedulePeriodicUpdateChecks(&update_attempter);

  // Run the main loop until exit time:
  g_main_loop_run(loop);

  // Cleanup:
  g_main_loop_unref(loop);
  update_attempter.set_dbus_service(NULL);
  g_object_unref(G_OBJECT(service));

  LOG(INFO) << "Chrome OS Update Engine terminating";
  return 0;
}
