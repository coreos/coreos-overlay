# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4

DESCRIPTION="CrOS specific busybox configuration file."

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"

S=${WORKDIR}

src_install() {
	insinto /etc/portage/savedconfig/sys-apps
	doins "${FILESDIR}/busybox"
}
