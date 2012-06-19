// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_CONNECTION_MANAGER_H_
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_CONNECTION_MANAGER_H_

#include <gmock/gmock.h>

#include "update_engine/connection_manager.h"

namespace chromeos_update_engine {

// This class mocks the generic interface to the connection manager
// (e.g FlimFlam, Shill, etc.) to consolidate all connection-related
// logic in update_engine.
class MockConnectionManager : public ConnectionManager {
 public:
  MockConnectionManager(SystemState* system_state)
      : ConnectionManager(system_state) {}

  MOCK_CONST_METHOD2(GetConnectionType, bool(DbusGlibInterface* dbus_iface,
                                             NetworkConnectionType* out_type));

  MOCK_CONST_METHOD1(IsUpdateAllowedOver, bool(NetworkConnectionType type));

  MOCK_CONST_METHOD1(StringForConnectionType,
      const char*(NetworkConnectionType type));
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_MOCK_CONNECTION_MANAGER_H_
