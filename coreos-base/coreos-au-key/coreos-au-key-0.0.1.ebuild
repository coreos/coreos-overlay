# Copyright (c) 2013 CoreOS Inc. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2

# This key pair is only used in development. An official production image will
# have the production keys inserted at build time. (see core_update_upload)

DESCRIPTION="CoreOS development auto update keys."
HOMEPAGE="http://github.com/coreos/"
SRC_URI=""
LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="cros_host"

src_install() {
	KEY_DIR=/usr/share/update_engine/
	insinto "${KEY_DIR}"
	doins "${FILESDIR}"/update-payload-key.pub.pem
	if use cros_host; then
		doins "${FILESDIR}"/update-payload-key.key.pem
	fi
}
