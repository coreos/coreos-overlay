# Copyright 2014 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI="5"
ETYPE="sources"
inherit kernel-2
detect_version

DESCRIPTION="Full sources for the CoreOS Linux kernel"
HOMEPAGE="http://www.kernel.org"
SRC_URI="${KERNEL_URI}"

KEYWORDS="amd64 arm64"
IUSE=""
