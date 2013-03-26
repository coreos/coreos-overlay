// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_CONNECTION_MANAGER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_CONNECTION_MANAGER_H_

#include <base/basictypes.h>

#include "update_engine/dbus_interface.h"

namespace chromeos_update_engine {

enum NetworkConnectionType {
  kNetEthernet = 0,
  kNetWifi,
  kNetWimax,
  kNetBluetooth,
  kNetCellular,
  kNetUnknown
};

class SystemState;

// This class exposes a generic interface to the connection manager
// (e.g FlimFlam, Shill, etc.) to consolidate all connection-related
// logic in update_engine.
class ConnectionManager {
 public:
  // Constructs a new ConnectionManager object initialized with the
  // given system state.
  explicit ConnectionManager(SystemState* system_state);

  // Populates |out_type| with the type of the network connection
  // that we are currently connected. The dbus_iface is used to
  // query the real connection manager (e.g shill).
  virtual bool GetConnectionType(DbusGlibInterface* dbus_iface,
                                 NetworkConnectionType* out_type) const;

  // Returns true if we're allowed to update the system when we're
  // connected to the internet through the given network connection type.
  virtual bool IsUpdateAllowedOver(NetworkConnectionType type) const;

  // Returns the string representation corresponding to the given
  // connection type.
  virtual const char* StringForConnectionType(NetworkConnectionType type) const;

 private:
  // The global context for update_engine
  SystemState* system_state_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionManager);
};

class NoopConnectionManager : public ConnectionManager {
 public:
  NoopConnectionManager(SystemState *system_state)
    : ConnectionManager(system_state) {
    return;
  }

  virtual bool GetConnectionType(
      DbusGlibInterface* dbus_iface,
      NetworkConnectionType* out_type) const {
    *out_type = kNetEthernet;
    return true;
  }

  virtual const char* StringForConnectionType(
      NetworkConnectionType type) const {
    return "Ethernet";
  }

  virtual bool IsUpdateAllowedOver(NetworkConnectionType type) const {
    return true;
  }
};


}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_CONNECTION_MANAGER_H_
