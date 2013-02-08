#!/bin/sh
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script is given one argument: the base of the source directory of
# the package, and it prints a string on stdout with the numerical version
# number for said repo.
#
# The reason we extract the version from the ChangeLog instead of BASE-VER is
# because BASE-VER contains a custom google string that lacks the x.y.z info.


exec awk '$1 == "*" && $2 == "GCC" && $4 == "released." { print $3; exit }' \
  "$1"/ChangeLog
