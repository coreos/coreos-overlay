// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_ACTION_PROCESSOR_MOCK_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_ACTION_PROCESSOR_MOCK_H__

#include <gmock/gmock.h>

#include "update_engine/action.h"

namespace chromeos_update_engine {

class ActionProcessorMock : public ActionProcessor {
 public:
  MOCK_METHOD0(StartProcessing, void());
  MOCK_METHOD1(EnqueueAction, void(AbstractAction* action));
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_ACTION_PROCESSOR_MOCK_H__
