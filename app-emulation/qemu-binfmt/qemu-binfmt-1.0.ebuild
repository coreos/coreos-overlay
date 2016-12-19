# Copyright (c) 2016 The CoreOS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=6

DESCRIPTION="QEMU binfmt support"
HOMEPAGE="https://coreos.com"
KEYWORDS="amd64"

LICENSE="Apache-2.0"
IUSE=""
SLOT=0

DEPEND=""
RDEPEND=""

S=${WORKDIR}

src_install() {
	insinto /etc/binfmt.d
	doins "${FILESDIR}"/qemu-aarch64.conf
}
