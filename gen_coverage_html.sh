#!/bin/bash

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex

scons debug=1 -j $(cat /proc/cpuinfo |grep '^processor' | wc -l)
lcov -d . --zerocounters
./update_engine_unittests --gtest_filter='-*.RunAsRoot*:*.Fakeroot*'
fakeroot ./update_engine_unittests --gtest_filter='*.Fakeroot*'
sudo ./update_engine_unittests --gtest_filter='*.RunAsRoot*'
lcov --directory . --capture --output-file app.info

# some versions of genhtml support the --no-function-coverage argument,
# which we want. The problem w/ function coverage is that every template
# instantiation of a method counts as a different method, so if we
# instantiate a method twice, once for testing and once for prod, the method
# is tested, but it shows only 50% function coverage b/c it thinks we didn't
# test the prod version.
genhtml --no-function-coverage -o html ./app.info || genhtml -o html ./app.info
./local_coverage_rate.sh
