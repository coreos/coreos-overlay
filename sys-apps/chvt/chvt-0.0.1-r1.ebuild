# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"

# A stub to install "chvt" utility from sys-apps/kbd into sbin.

DESCRIPTION="chvt: Change virtual terminal console"
LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm x86"

DEPEND="sys-apps/kbd"
RDEPEND=""

# There's no source (required for EAPI=4)
S="${WORKDIR}"

src_install() {
	dosbin ${ROOT}/usr/bin/chvt
}
