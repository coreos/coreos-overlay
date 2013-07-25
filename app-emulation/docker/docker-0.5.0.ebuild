#
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=2
EGIT_REPO_URI="https://github.com/dotcloud/docker"
inherit toolchain-funcs systemd git-2

EGIT_COMMIT="51f6c4a7372450d164c61e0054daf0223ddbd909" # 0.5

DESCRIPTION="Docker container management"
HOMEPAGE="http://docker.io"
SRC_URI=""

LICENSE="MIT"
SLOT="0"
KEYWORDS="amd64"
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
	# not enabling by default because it messes up the EC2 169. meta url routing
	#systemd_enable_service multi-user.target ${PN}.service
}
