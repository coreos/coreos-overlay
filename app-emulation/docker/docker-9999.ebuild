#
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=2
EGIT_REPO_URI="https://github.com/dotcloud/docker"
inherit toolchain-funcs systemd git-2

DESCRIPTION="Docker container management"
HOMEPAGE="http://docker.io"
SRC_URI=""

LICENSE="MIT"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE=""

DEPEND=">=dev-lang/go-1.0.2"
RDEPEND="
	app-emulation/lxc
	net-misc/bridge-utils
	sys-apps/iproute2
	app-arch/libarchive
	net-misc/curl
	sys-fs/aufs-util
"

src_compile() {
	emake
}

src_install() {
	dobin ${S}/bin/${PN}
	keepdir /var/lib/${PN}/graph
	keepdir /var/lib/${PN}/containers
	systemd_dounit "${FILESDIR}"/${PN}.service
	systemd_enable_service multi-user.target ${PN}.service
}
