# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

EAPI="4"

DESCRIPTION="Convenience script for testing attached cell modems"

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

RDEPEND="dev-util/shflags
	net-misc/socat"
DEPEND=""

S=${WORKDIR}

src_install() {
	dobin "${FILESDIR}"/modem-diagnostics
	dobin "${FILESDIR}"/qpst_setup
}
