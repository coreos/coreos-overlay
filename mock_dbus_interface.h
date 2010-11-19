// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_DBUS_INTERFACE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_DBUS_INTERFACE_H__

#include <gmock/gmock.h>

#include "update_engine/dbus_interface.h"

namespace chromeos_update_engine {

class MockDbusGlib : public DbusGlibInterface {
 public:
  MOCK_METHOD5(ProxyNewForNameOwner, DBusGProxy*(DBusGConnection *connection,
                                                 const char *name,
                                                 const char *path,
                                                 const char *interface,
                                                 GError **error));

  MOCK_METHOD1(ProxyUnref, void(DBusGProxy* proxy));

  MOCK_METHOD2(BusGet, DBusGConnection*(DBusBusType type, GError **error));

  MOCK_METHOD7(ProxyCall, gboolean(DBusGProxy *proxy,
                                   const char *method,
                                   GError **error,
                                   GType first_arg_type,
                                   GType var_arg1,
                                   GHashTable** var_arg2,
                                   GType var_arg3));
  MOCK_METHOD10(ProxyCall, gboolean(DBusGProxy* proxy,
                                    const char* method,
                                    GError** error,
                                    GType var_arg1, const char* var_arg2,
                                    GType var_arg3,
                                    GType var_arg4, gchar** var_arg5,
                                    GType var_arg6, GArray** var_arg7));

  // Since gmock only supports mocking functions up to 10 args, we
  // take the 11-arg function we'd like to mock, drop the last arg
  // and call the 10-arg version. Dropping the last arg isn't ideal,
  // but this is a lot better than nothing.
  gboolean ProxyCall(DBusGProxy* proxy,
                       const char* method,
                       GError** error,
                       GType var_arg1, const char* var_arg2,
                       GType var_arg3,
                       GType var_arg4, gchar** var_arg5,
                       GType var_arg6, GArray** var_arg7,
                       GType var_arg8) {
    return ProxyCall(proxy,
                     method,
                     error,
                     var_arg1, var_arg2,
                     var_arg3,
                     var_arg4, var_arg5,
                     var_arg6, var_arg7);
  }
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_DBUS_INTERFACE_H__
