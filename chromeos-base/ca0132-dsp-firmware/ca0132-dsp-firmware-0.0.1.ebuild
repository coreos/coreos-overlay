# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4

DESCRIPTION="Ebuild that installs the firmware needed by the ca0132 codec."

LICENSE="BSD"
SLOT="0"
KEYWORDS="x86 amd64"

DSP_FW_NAME="ctefx.bin"
EQ_NAME="ctspeq.bin"

S=${WORKDIR}

src_install() {
	insinto /lib/firmware
	doins "${FILESDIR}/${DSP_FW_NAME}"
	doins "${FILESDIR}/${EQ_NAME}"
}
