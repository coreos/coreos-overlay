// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/connection_manager.h"

#include <string>

#include <base/stl_util.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/dbus-glib.h>
#include <glib.h>

#include "update_engine/system_state.h"
#include "update_engine/utils.h"

using std::set;
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
                                           flimflam::kFlimflamServiceName,
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
                                      flimflam::kFlimflamServicePath,
                                      flimflam::kFlimflamManagerInterface,
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
  if (!strcmp(type_str, flimflam::kTypeEthernet)) {
    return kNetEthernet;
  } else if (!strcmp(type_str, flimflam::kTypeWifi)) {
    return kNetWifi;
  } else if (!strcmp(type_str, flimflam::kTypeWimax)) {
    return kNetWimax;
  } else if (!strcmp(type_str, flimflam::kTypeBluetooth)) {
    return kNetBluetooth;
  } else if (!strcmp(type_str, flimflam::kTypeCellular)) {
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
                                      flimflam::kFlimflamServiceInterface,
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

ConnectionManager::ConnectionManager(SystemState *system_state)
    :  system_state_(system_state) {}

bool ConnectionManager::IsUpdateAllowedOver(NetworkConnectionType type) const {
  switch (type) {
    case kNetBluetooth:
      return false;

    case kNetCellular: {
      set<string> allowed_types;
      const policy::DevicePolicy* device_policy =
          system_state_->device_policy();
      if (!device_policy) {
        LOG(INFO) << "Disabling updates over cellular connection as there's no "
                     "device policy object present";
        return false;
      }

      if (!device_policy->GetAllowedConnectionTypesForUpdate(&allowed_types)) {
        LOG(INFO) << "Disabling updates over cellular connection as there's no "
                     "allowed connection types from policy";
        return false;
      }

      if (!ContainsKey(allowed_types, flimflam::kTypeCellular)) {
        LOG(INFO) << "Disabling updates over cellular connection as it's not "
                     "allowed in the device policy.";
        return false;
      }

      LOG(INFO) << "Allowing updates over cellular per device policy";
      return true;
    }

    default:
      return true;
  }
}

const char* ConnectionManager::StringForConnectionType(
    NetworkConnectionType type) const {
  static const char* const kValues[] = {flimflam::kTypeEthernet,
                                        flimflam::kTypeWifi,
                                        flimflam::kTypeWimax,
                                        flimflam::kTypeBluetooth,
                                        flimflam::kTypeCellular};
  if (type < 0 || type >= static_cast<int>(arraysize(kValues))) {
    return "Unknown";
  }
  return kValues[type];
}

bool ConnectionManager::GetConnectionType(
    DbusGlibInterface* dbus_iface,
    NetworkConnectionType* out_type) const {
  string default_service_path;
  TEST_AND_RETURN_FALSE(GetDefaultServicePath(dbus_iface,
                                              &default_service_path));
  TEST_AND_RETURN_FALSE(GetServicePathType(dbus_iface,
                                           default_service_path,
                                           out_type));
  return true;
}

}  // namespace chromeos_update_engine
