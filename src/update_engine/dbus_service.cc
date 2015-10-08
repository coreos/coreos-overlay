// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/dbus_service.h"

#include <string>

#include <glog/logging.h>

#include "update_engine/marshal.glibmarshal.h"
#include "update_engine/omaha_request_params.h"
#include "update_engine/utils.h"

using std::string;

G_DEFINE_TYPE(UpdateEngineService, update_engine_service, G_TYPE_OBJECT)

static void update_engine_service_finalize(GObject* object) {
  G_OBJECT_CLASS(update_engine_service_parent_class)->finalize(object);
}

static guint status_update_signal = 0;

static void update_engine_service_class_init(UpdateEngineServiceClass* klass) {
  GObjectClass *object_class;
  object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = update_engine_service_finalize;

  status_update_signal = g_signal_new(
      "status_update",
      G_OBJECT_CLASS_TYPE(klass),
      G_SIGNAL_RUN_LAST,
      0,  // 0 == no class method associated
      NULL,  // Accumulator
      NULL,  // Accumulator data
      update_engine_VOID__INT64_DOUBLE_STRING_STRING_INT64,
      G_TYPE_NONE,  // Return type
      5,  // param count:
      G_TYPE_INT64,
      G_TYPE_DOUBLE,
      G_TYPE_STRING,
      G_TYPE_STRING,
      G_TYPE_INT64);
}

static void update_engine_service_init(UpdateEngineService* object) {
}

UpdateEngineService* update_engine_service_new(void) {
  return reinterpret_cast<UpdateEngineService*>(
      g_object_new(UPDATE_ENGINE_TYPE_SERVICE, NULL));
}

gboolean update_engine_service_attempt_update(UpdateEngineService* self,
                                              GError **error) {
  LOG(INFO) << "Attempting interactive update";
  self->system_state_->update_attempter()->CheckForUpdate(true);
  return TRUE;
}

gboolean update_engine_service_reset_status(UpdateEngineService* self,
                                            GError **error) {
  *error = NULL;
  return self->system_state_->update_attempter()->ResetStatus();
}


gboolean update_engine_service_get_status(UpdateEngineService* self,
                                          int64_t* last_checked_time,
                                          double* progress,
                                          gchar** current_operation,
                                          gchar** new_version,
                                          int64_t* new_size,
                                          GError **error) {
  string current_op;
  string new_version_str;

  CHECK(self->system_state_->update_attempter()->GetStatus(last_checked_time,
                                                           progress,
                                                           &current_op,
                                                           &new_version_str,
                                                           new_size));

  *current_operation = g_strdup(current_op.c_str());
  *new_version = g_strdup(new_version_str.c_str());
  if (!(*current_operation && *new_version)) {
    *error = NULL;
    return FALSE;
  }
  return TRUE;
}

gboolean update_engine_service_emit_status_update(
    UpdateEngineService* self,
    gint64 last_checked_time,
    gdouble progress,
    const gchar* current_operation,
    const gchar* new_version,
    gint64 new_size) {
  g_signal_emit(self,
                status_update_signal,
                0,
                last_checked_time,
                progress,
                current_operation,
                new_version,
                new_size);
  return TRUE;
}
