// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_INTERFACE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_INTERFACE_H__

// This class interfaces with DBus. The interface allows it to be mocked.

#include <dbus/dbus-glib.h>

namespace chromeos_update_engine {

class DbusGlibInterface {
 public:
  // wraps dbus_g_proxy_new_for_name_owner
  virtual DBusGProxy* ProxyNewForNameOwner(DBusGConnection* connection,
                                           const char* name,
                                           const char* path,
                                           const char* interface,
                                           GError** error) = 0;

  // wraps g_object_unref
  virtual void ProxyUnref(DBusGProxy* proxy) = 0;

  // wraps dbus_g_bus_get
  virtual DBusGConnection*  BusGet(DBusBusType type, GError** error) = 0;
  
  // wraps dbus_g_proxy_call
  virtual gboolean ProxyCall(DBusGProxy* proxy,
                             const char* method,
                             GError** error,
                             GType first_arg_type,
                             GType var_arg1,
                             GHashTable** var_arg2,
                             GType var_arg3) = 0;
};

class ConcreteDbusGlib : public DbusGlibInterface {
  virtual DBusGProxy* ProxyNewForNameOwner(DBusGConnection* connection,
                                           const char* name,
                                           const char* path,
                                           const char* interface,
                                           GError** error) {
    return dbus_g_proxy_new_for_name_owner(connection,
                                           name,
                                           path,
                                           interface,
                                           error);
  }

  virtual void ProxyUnref(DBusGProxy* proxy) {
    g_object_unref(proxy);
  }

  virtual DBusGConnection*  BusGet(DBusBusType type, GError** error) {
    return dbus_g_bus_get(type, error);
  }
  
  virtual gboolean ProxyCall(DBusGProxy* proxy,
                             const char* method,
                             GError** error,
                             GType first_arg_type,
                             GType var_arg1,
                             GHashTable** var_arg2,
                             GType var_arg3) {
    return dbus_g_proxy_call(
        proxy, method, error, first_arg_type, var_arg1, var_arg2, var_arg3);
  }
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_INTERFACE_H__
