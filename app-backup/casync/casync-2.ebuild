# Copyright 1999-2017 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

DESCRIPTION="casync - Content-Addressable Data Synchronization Tool - is a tool similar to rsync for backing up and updating directory trees"
HOMEPAGE="https://github.com/systemd/casync"
SRC_URI="https://github.com/systemd/casync/archive/v${PV}.tar.gz"

inherit meson eutils

LICENSE="LGPL-2.1"
SLOT="0"
KEYWORDS="amd64"
IUSE="fuse selinux doc"

DEPEND="app-arch/zstd
fuse? ( sys-fs/fuse )
"
RDEPEND="${DEPEND}"

PATCHES=(
	"${FILESDIR}/fix-selinux-build.patch"
)

src_configure() {
	local emesonargs=(
		-Dfuse=$(usex fuse true false)
		-Dselinux=$(usex selinux true false)
		-Dman=$(usex doc true false)
	)
	meson_src_configure
}
