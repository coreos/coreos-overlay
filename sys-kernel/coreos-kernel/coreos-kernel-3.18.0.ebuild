# Copyright 2014 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
COREOS_SOURCE_REVISION=""
COREOS_TARGET_BOARD="hikey"
COREOS_TARGET_DTB="hi6220-hikey.dtb"

inherit coreos-kernel

DESCRIPTION="linux-3.18 kernel for Hikey board"
KEYWORDS="arm64"
