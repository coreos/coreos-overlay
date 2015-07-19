// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/delta_metadata.h"

namespace chromeos_update_engine {

const char kDeltaMagic[] = "CrAU";
static_assert(kDeltaMagicSize == sizeof(kDeltaMagic) - 1, "invalid size");

};  // namespace chromeos_update_engine
