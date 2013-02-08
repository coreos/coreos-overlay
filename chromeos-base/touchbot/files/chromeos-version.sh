#!/bin/sh

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

exec sed -nr "/version=/{s:.*='(.*)'.*:\1:;p}" "$1/setup.py"
