// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_INTERFACE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_INTERFACE_H__

// This class interfaces with DBus. The interface allows it to be mocked.

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glog/logging.h>

#ifndef DBUS_TYPE_G_OBJECT_PATH_ARRAY
#define DBUS_TYPE_G_OBJECT_PATH_ARRAY \
  (dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_OBJECT_PATH))
#endif

#ifndef DBUS_TYPE_G_STRING_ARRAY
#define DBUS_TYPE_G_STRING_ARRAY \
  (dbus_g_type_get_collection ("GPtrArray", G_TYPE_STRING))
#endif

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
  virtual DBusGConnection* BusGet(DBusBusType type, GError** error) = 0;

  // wraps dbus_g_proxy_call
  virtual gboolean ProxyCall(DBusGProxy* proxy,
                             const char* method,
                             GError** error,
                             GType first_arg_type,
                             GType var_arg1,
                             GHashTable** var_arg2,
                             GType var_arg3) = 0;

  virtual gboolean ProxyCall(DBusGProxy* proxy,
                             const char* method,
                             GError** error,
                             GType var_arg1, const char* var_arg2,
                             GType var_arg3,
                             GType var_arg4, gchar** var_arg5,
                             GType var_arg6, GArray** var_arg7,
                             GType var_arg8) = 0;

  virtual gboolean ProxyCall(DBusGProxy* proxy,
                             const char* method,
                             GError** error,
                             GType var_arg1, const char* var_arg2,
                             GType var_arg3, const char* var_arg4,
                             GType var_arg5, const char* var_arg6,
                             GType var_arg7, GType var_arg8) = 0;

  virtual DBusConnection* ConnectionGetConnection(DBusGConnection* gbus) = 0;

  virtual void DbusBusAddMatch(DBusConnection* connection,
                               const char* rule,
                               DBusError* error) = 0;

  virtual dbus_bool_t DbusConnectionAddFilter(
      DBusConnection* connection,
      DBusHandleMessageFunction function,
      void* user_data,
      DBusFreeFunction free_data_function) = 0;
  virtual void DbusConnectionRemoveFilter(DBusConnection* connection,
                                          DBusHandleMessageFunction function,
                                          void* user_data) = 0;
  virtual dbus_bool_t DbusMessageIsSignal(DBusMessage* message,
                                          const char* interface,
                                          const char* signal_name) = 0;
  virtual dbus_bool_t DbusMessageGetArgs(DBusMessage* message,
                                         DBusError* error,
                                         GType var_arg1, char** var_arg2,
                                         GType var_arg3, char** var_arg4,
                                         GType var_arg5, char** var_arg6,
                                         GType var_arg7) = 0;
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

  virtual DBusGConnection* BusGet(DBusBusType type, GError** error) {
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

  virtual gboolean ProxyCall(DBusGProxy* proxy,
                             const char* method,
                             GError** error,
                             GType var_arg1, const char* var_arg2,
                             GType var_arg3,
                             GType var_arg4, gchar** var_arg5,
                             GType var_arg6, GArray** var_arg7,
                             GType var_arg8) {
    return dbus_g_proxy_call(
        proxy, method, error, var_arg1, var_arg2, var_arg3,
        var_arg4, var_arg5, var_arg6, var_arg7, var_arg8);
  }

  virtual gboolean ProxyCall(DBusGProxy* proxy,
                             const char* method,
                             GError** error,
                             GType var_arg1, const char* var_arg2,
                             GType var_arg3, const char* var_arg4,
                             GType var_arg5, const char* var_arg6,
                             GType var_arg7, GType var_arg8) {
    return dbus_g_proxy_call(
        proxy, method, error, var_arg1, var_arg2, var_arg3,
        var_arg4, var_arg5, var_arg6, var_arg7, var_arg8);
  }


  virtual DBusConnection* ConnectionGetConnection(DBusGConnection* gbus) {
    return dbus_g_connection_get_connection(gbus);
  }

  virtual void DbusBusAddMatch(DBusConnection* connection,
                               const char* rule,
                               DBusError* error) {
    dbus_bus_add_match(connection, rule, error);
  }

  virtual dbus_bool_t DbusConnectionAddFilter(
      DBusConnection* connection,
      DBusHandleMessageFunction function,
      void* user_data,
      DBusFreeFunction free_data_function) {
    return dbus_connection_add_filter(connection,
                                      function,
                                      user_data,
                                      free_data_function);
  }
  virtual void DbusConnectionRemoveFilter(DBusConnection* connection,
                                          DBusHandleMessageFunction function,
                                          void* user_data) {
    dbus_connection_remove_filter(connection, function, user_data);
  }
  dbus_bool_t DbusMessageIsSignal(DBusMessage* message,
                                  const char* interface,
                                  const char* signal_name) {
    return dbus_message_is_signal(message, interface, signal_name);
  }
  virtual dbus_bool_t DbusMessageGetArgs(DBusMessage* message,
                                         DBusError* error,
                                         GType var_arg1, char** var_arg2,
                                         GType var_arg3, char** var_arg4,
                                         GType var_arg5, char** var_arg6,
                                         GType var_arg7) {
    return dbus_message_get_args(message, error,
                                 var_arg1, var_arg2,
                                 var_arg3, var_arg4,
                                 var_arg5, var_arg6,
                                 var_arg7);
  }
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_INTERFACE_H__
