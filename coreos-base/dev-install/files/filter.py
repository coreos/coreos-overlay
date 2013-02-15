#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Filter out all the packages that are already in coreos.
cros_pkgs = set(open('coreos.packages', 'r').readlines())
port_pkgs = set(open('portage.packages', 'r').readlines())

boot_pkgs = port_pkgs - cros_pkgs
f = open('bootstrap.packages', 'w')
f.write(''.join(boot_pkgs))
f.close()

# After bootstrapping the package will be assumed
# to be installed by emerge.
prov_pkgs = [x for x in boot_pkgs if not x.startswith('virtual/')]
f = open('package.provided', 'a')
f.write(''.join(prov_pkgs))
f.close()

# Make a list of the packages that can be installed.  Those packages
# are in coreos-dev or coreos-test and not coreos.
dev_pkgs = set(open('coreos-dev.packages', 'r').readlines())
#test_pkgs = set(open('coreos-test.packages', 'r').readlines())
test_pkgs = set()
inst_pkgs = (dev_pkgs | test_pkgs) - cros_pkgs
f = open('package.installable', 'w')
f.write(''.join(inst_pkgs))
f.close()
