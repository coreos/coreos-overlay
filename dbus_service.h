// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_SERVICE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_SERVICE_H__

#include <inttypes.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib-object.h>

#include "update_engine/update_attempter.h"

// Type macros:
#define UPDATE_ENGINE_TYPE_SERVICE (update_engine_service_get_type())
#define UPDATE_ENGINE_SERVICE(obj)                                      \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), UPDATE_ENGINE_TYPE_SERVICE,        \
                              UpdateEngineService))
#define UPDATE_ENGINE_IS_SERVICE(obj)                                   \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), UPDATE_ENGINE_TYPE_SERVICE))
#define UPDATE_ENGINE_SERVICE_CLASS(klass)                      \
  (G_TYPE_CHECK_CLASS_CAST((klass), UPDATE_ENGINE_TYPE_SERVICE, \
                           UpdateEngineService))
#define UPDATE_ENGINE_IS_SERVICE_CLASS(klass)                           \
  (G_TYPE_CHECK_CLASS_TYPE((klass), UPDATE_ENGINE_TYPE_SERVICE))
#define UPDATE_ENGINE_SERVICE_GET_CLASS(obj)                    \
  (G_TYPE_INSTANCE_GET_CLASS((obj), UPDATE_ENGINE_TYPE_SERVICE, \
                             UpdateEngineService))

G_BEGIN_DECLS

struct UpdateEngineService {
  GObject parent_instance;

  chromeos_update_engine::UpdateAttempter* update_attempter_;
};

struct UpdateEngineServiceClass {
  GObjectClass parent_class;
};

UpdateEngineService* update_engine_service_new(void);
GType update_engine_service_get_type(void);

// Methods

gboolean update_engine_service_attempt_update(UpdateEngineService* self,
                                              gchar* app_version,
                                              gchar* omaha_url,
                                              GError **error);

gboolean update_engine_service_get_status(UpdateEngineService* self,
                                          int64_t* last_checked_time,
                                          double* progress,
                                          gchar** current_operation,
                                          gchar** new_version,
                                          int64_t* new_size,
                                          GError **error);

gboolean update_engine_service_reboot_if_needed(UpdateEngineService* self,
                                                GError **error);

gboolean update_engine_service_set_track(UpdateEngineService* self,
                                         gchar* track,
                                         GError **error);

gboolean update_engine_service_emit_status_update(
    UpdateEngineService* self,
    gint64 last_checked_time,
    gdouble progress,
    const gchar* current_operation,
    const gchar* new_version,
    gint64 new_size);

G_END_DECLS

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_SERVICE_H__
