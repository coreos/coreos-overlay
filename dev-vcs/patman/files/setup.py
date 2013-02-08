#!/usr/bin/env python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from distutils.core import setup

setup(name='patman',
      version='121220',
      url='http://www.denx.de/wiki/U-Boot',
      packages=['patman'],
      package_dir={'patman' : '.'})
