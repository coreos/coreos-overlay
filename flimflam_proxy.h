// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_FLIMFLAM_PROXY_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_FLIMFLAM_PROXY_H__

// This class interfaces with FlimFlam to find out data about connectivity.

#include <base/basictypes.h>

#include "update_engine/dbus_interface.h"

namespace chromeos_update_engine {

extern const char* kFlimFlamDbusService;
extern const char* kFlimFlamDbusManagerInterface;
extern const char* kFlimFlamDbusManagerPath;
extern const char* kFlimFlamDbusServiceInterface;

extern const char* kFlimFlamNetTypeEthernet;
extern const char* kFlimFlamNetTypeWifi;
extern const char* kFlimFlamNetTypeWimax;
extern const char* kFlimFlamNetTypeBluetooth;
extern const char* kFlimFlamNetTypeCellular;

enum NetworkConnectionType {
  kNetEthernet = 0,
  kNetWifi,
  kNetWimax,
  kNetBluetooth,
  kNetCellular,
  kNetUnknown
};

class FlimFlamProxy {
 public:
  static bool GetConnectionType(DbusGlibInterface* dbus_iface,
                                NetworkConnectionType* out_type);

  static bool IsExpensiveConnectionType(NetworkConnectionType type) {
    return type == kNetWimax || type == kNetBluetooth || type == kNetCellular;
  }
  static const char* StringForConnectionType(NetworkConnectionType type);
  
 private:
  // Should never be allocated
  DISALLOW_IMPLICIT_CONSTRUCTORS(FlimFlamProxy);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_FLIMFLAM_PROXY_H__
