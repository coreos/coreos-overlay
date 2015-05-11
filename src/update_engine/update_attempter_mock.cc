// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_attempter_mock.h"

#include "update_engine/mock_system_state.h"

namespace chromeos_update_engine {

UpdateAttempterMock::UpdateAttempterMock(MockSystemState* mock_system_state,
                                         MockDbusGlib* dbus)
    : UpdateAttempter(mock_system_state, dbus) {}

}  // namespace chromeos_update_engine
