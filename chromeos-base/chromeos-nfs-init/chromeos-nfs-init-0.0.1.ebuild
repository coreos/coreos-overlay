# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2

inherit toolchain-funcs

DESCRIPTION="Upstart init scripts for NFS on Chromium OS"
HOMEPAGE="http://src.chromium.org"
SRC_URI=""
LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"

DEPEND=""
RDEPEND="net-fs/nfs-utils
	sys-apps/upstart"
IUSE="nfs"

src_install() {
	# Install our NFS configuration files.
	dodir /etc/init
	install --owner=root --group=root --mode=0644 \
		"${FILESDIR}"/*.conf "${D}/etc/init/"

	dodir /etc/init/lib
	install --owner=root --group=root --mode=0755 \
		"${FILESDIR}"/nfs-check-setup "${D}/etc/init/lib"
}
