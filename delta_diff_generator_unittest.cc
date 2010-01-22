// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <set>
#include <string>
#include <vector>
#include "base/string_util.h"
#include <gtest/gtest.h>
#include "chromeos/obsolete_logging.h"
#include "update_engine/decompressing_file_writer.h"
#include "update_engine/delta_diff_generator.h"
#include "update_engine/delta_diff_parser.h"
#include "update_engine/gzip.h"
#include "update_engine/mock_file_writer.h"
#include "update_engine/subprocess.h"
#include "update_engine/test_utils.h"
#include "update_engine/utils.h"

namespace chromeos_update_engine {

class DeltaDiffGeneratorTest : public ::testing::Test {};

}  // namespace chromeos_update_engine
