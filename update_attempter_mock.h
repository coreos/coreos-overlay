// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_ATTEMPTER_MOCK_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_ATTEMPTER_MOCK_H__

#include <gmock/gmock.h>

#include "update_engine/mock_dbus_interface.h"
#include "update_engine/update_attempter.h"

namespace chromeos_update_engine {

class UpdateAttempterMock : public UpdateAttempter {
 public:
  UpdateAttempterMock() : UpdateAttempter(NULL, NULL, &dbus_) {}

  MOCK_METHOD3(Update, void(const std::string& app_version,
                            const std::string& omaha_url,
                            bool obey_proxies));
  MockDbusGlib dbus_;
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_UPDATE_ATTEMPTER_MOCK_H__
