// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/flimflam_proxy.h"

#include <string>

#include <base/string_util.h>
#include <dbus/dbus-glib.h>
#include <glib.h>

#include "update_engine/utils.h"

using std::string;

namespace chromeos_update_engine {

namespace {

// Gets the DbusGProxy for FlimFlam. Must be free'd with ProxyUnref()
bool GetFlimFlamProxy(DbusGlibInterface* dbus_iface,
                      const char* path,
                      const char* interface,
                      DBusGProxy** out_proxy) {
  DBusGConnection* bus;
  DBusGProxy* proxy;
  GError* error = NULL;

  bus = dbus_iface->BusGet(DBUS_BUS_SYSTEM, &error);
  if (!bus) {
    LOG(ERROR) << "Failed to get system bus";
    return false;
  }
  proxy = dbus_iface->ProxyNewForNameOwner(bus,
                                           kFlimFlamDbusService,
                                           path,
                                           interface,
                                           &error);
  if (!proxy) {
    LOG(ERROR) << "Error getting FlimFlam proxy: "
               << utils::GetAndFreeGError(&error);
    return false;
  }
  *out_proxy = proxy;
  return true;
}

// On success, caller owns the GHashTable at out_hash_table.
// Returns true on success.
bool GetProperties(DbusGlibInterface* dbus_iface,
                   const char* path,
                   const char* interface,
                   GHashTable** out_hash_table) {
  DBusGProxy* proxy;
  GError* error = NULL;

  TEST_AND_RETURN_FALSE(GetFlimFlamProxy(dbus_iface,
                                         path,
                                         interface,
                                         &proxy));

  gboolean rc = dbus_iface->ProxyCall(proxy,
                                      "GetProperties",
                                      &error,
                                      G_TYPE_INVALID,
                                      dbus_g_type_get_map("GHashTable",
                                                          G_TYPE_STRING,
                                                          G_TYPE_VALUE),
                                      out_hash_table,
                                      G_TYPE_INVALID);
  dbus_iface->ProxyUnref(proxy);
  if (rc == FALSE) {
    LOG(ERROR) << "dbus_g_proxy_call failed";
    return false;
  }

  return true;
}

// Returns (via out_path) the default network path, or empty string if
// there's no network up.
// Returns true on success.
bool GetDefaultServicePath(DbusGlibInterface* dbus_iface, string* out_path) {
  GHashTable* hash_table = NULL;

  TEST_AND_RETURN_FALSE(GetProperties(dbus_iface,
                                      kFlimFlamDbusManagerPath,
                                      kFlimFlamDbusManagerInterface,
                                      &hash_table));

  GValue* value = reinterpret_cast<GValue*>(g_hash_table_lookup(hash_table,
                                                                "Services"));
  GArray* array = NULL;
  bool success = false;
  if (G_VALUE_HOLDS(value, DBUS_TYPE_G_OBJECT_PATH_ARRAY) &&
      (array = reinterpret_cast<GArray*>(g_value_get_boxed(value))) &&
      (array->len > 0)) {
    *out_path = g_array_index(array, const char*, 0);
    success = true;
  }

  g_hash_table_unref(hash_table);
  return success;
}

NetworkConnectionType ParseConnectionType(const char* type_str) {
  if (!strcmp(type_str, kFlimFlamNetTypeEthernet)) {
    return kNetEthernet;
  } else if (!strcmp(type_str, kFlimFlamNetTypeWifi)) {
    return kNetWifi;
  } else if (!strcmp(type_str, kFlimFlamNetTypeWimax)) {
    return kNetWimax;
  } else if (!strcmp(type_str, kFlimFlamNetTypeBluetooth)) {
    return kNetBluetooth;
  } else if (!strcmp(type_str, kFlimFlamNetTypeCellular)) {
    return kNetCellular;
  }
  return kNetUnknown;
}

bool GetServicePathType(DbusGlibInterface* dbus_iface,
                        const string& path,
                        NetworkConnectionType* out_type) {
  GHashTable* hash_table = NULL;

  TEST_AND_RETURN_FALSE(GetProperties(dbus_iface,
                                      path.c_str(),
                                      kFlimFlamDbusServiceInterface,
                                      &hash_table));

  GValue* value = (GValue*)g_hash_table_lookup(hash_table, "Type");
  const char* type_str = NULL;
  bool success = false;
  if (value != NULL && (type_str = g_value_get_string(value)) != NULL) {
    *out_type = ParseConnectionType(type_str);
    success = true;
  }
  g_hash_table_unref(hash_table);
  return success;
}

}  // namespace {}

const char* FlimFlamProxy::StringForConnectionType(NetworkConnectionType type) {
  static const char* const kValues[] = {kFlimFlamNetTypeEthernet,
                                        kFlimFlamNetTypeWifi,
                                        kFlimFlamNetTypeWimax,
                                        kFlimFlamNetTypeBluetooth,
                                        kFlimFlamNetTypeCellular};
  if (type < 0 || type >= static_cast<int>(arraysize(kValues))) {
    return "Unknown";
  }
  return kValues[type];
}

bool FlimFlamProxy::GetConnectionType(DbusGlibInterface* dbus_iface,
                                      NetworkConnectionType* out_type) {
  string default_service_path;
  TEST_AND_RETURN_FALSE(GetDefaultServicePath(dbus_iface,
                                              &default_service_path));
  TEST_AND_RETURN_FALSE(GetServicePathType(dbus_iface,
                                           default_service_path,
                                           out_type));
  return true;
}

const char* kFlimFlamDbusService = "org.chromium.flimflam";
const char* kFlimFlamDbusManagerInterface = "org.chromium.flimflam.Manager";
const char* kFlimFlamDbusManagerPath = "/";
const char* kFlimFlamDbusServiceInterface = "org.chromium.flimflam.Service";

const char* kFlimFlamNetTypeEthernet = "ethernet";
const char* kFlimFlamNetTypeWifi = "wifi";
const char* kFlimFlamNetTypeWimax = "wimax";
const char* kFlimFlamNetTypeBluetooth = "bluetooth";
const char* kFlimFlamNetTypeCellular = "cellular";

}  // namespace chromeos_update_engine
