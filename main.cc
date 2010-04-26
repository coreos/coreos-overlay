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
#include "update_engine/dbus_constants.h"
#include "update_engine/dbus_service.h"
#include "update_engine/update_attempter.h"

extern "C" {
#include "update_engine/update_engine.dbusserver.h"
}

DEFINE_bool(logtostderr, false,
            "Write logs to stderr instead of to a file in log_dir.");

using std::string;
using std::tr1::shared_ptr;
using std::vector;

namespace chromeos_update_engine {

gboolean SetupInMainLoop(void* arg) {
  // TODO(adlr): Tell update_attempter to start working.
  // Comment this in for that:
  //UpdateAttempter* update_attempter = reinterpret_cast<UpdateAttempter*>(arg);

  return FALSE;  // Don't call this callback function again
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
    exit(1);
    LOG(FATAL) << "Got result code " << request_name_ret
               << " from requesting name, but expected "
               << DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER;
  }
  dbus_g_connection_register_g_object(bus,
                                      "/org/chromium/UpdateEngine",
                                      G_OBJECT(service));
}

}  // namespace chromeos_update_engine

#include "update_engine/subprocess.h"

int main(int argc, char** argv) {
  ::g_type_init();
  g_thread_init(NULL);
  dbus_g_thread_init();
  chromeos_update_engine::Subprocess::Init();
  google::ParseCommandLineFlags(&argc, &argv, true);
  CommandLine::Init(argc, argv);
  logging::InitLogging("logfile.txt",
                       FLAGS_logtostderr ?
                         logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG :
                         logging::LOG_ONLY_TO_FILE,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
  LOG(INFO) << "Chrome OS Update Engine starting";
  
  // Create the single GMainLoop
  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);

  // Create the update attempter:
  chromeos_update_engine::UpdateAttempter update_attempter(loop);

  // Create the dbus service object:
  dbus_g_object_type_install_info(UPDATE_ENGINE_TYPE_SERVICE,
                                  &dbus_glib_update_engine_service_object_info);
  UpdateEngineService* service =
      UPDATE_ENGINE_SERVICE(g_object_new(UPDATE_ENGINE_TYPE_SERVICE, NULL));
  service->update_attempter_ = &update_attempter;
  chromeos_update_engine::SetupDbusService(service);

  // Set up init routine to run within the main loop.
  g_timeout_add(0, &chromeos_update_engine::SetupInMainLoop, &update_attempter);

  // Run the main loop until exit time:
  g_main_loop_run(loop);

  // Cleanup:
  g_main_loop_unref(loop);
  g_object_unref(G_OBJECT(service));

  LOG(INFO) << "Chrome OS Update Engine terminating";
  return 0;
}
