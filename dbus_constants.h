// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_CONSTANTS_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_CONSTANTS_H__

namespace chromeos_update_engine {

static const char* const kUpdateEngineServiceName = "com.coreos.update1";
static const char* const kUpdateEngineServicePath =
    "/com/coreos/update1";
static const char* const kUpdateEngineServiceInterface =
    "com.coreos.update1.Manager";
}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_CONSTANTS_H__
