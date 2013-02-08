#!/bin/sh
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script is given one argument: the base of the source directory of
# the package, and it prints a string on stdout with the numerical version
# number for said repo.

# Create a temporary makefile:
#  - declare a dummy "e" target (which u-boot itself doesn't define)
#  - that echos out the u-boot version
#  - include the main u-boot makefile
# This lets the u-boot makefile logic do all its crazy stuff without
# worrying about it (which we would if we tried to sed/grep it out).
#
# The make command will read the temporary makefile from stdin and
# parse the e target.
printf 'e:;@echo $(U_BOOT_VERSION)\ninclude '$1'/Makefile\n' | make -f - e
