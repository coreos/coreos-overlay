# Copyright 2013 The CoreOS Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=5

DESCRIPTION="Docker container management"
HOMEPAGE="http://docker.io"
SRC_URI="https://github.com/dotcloud/docker/archive/v${PV}.tar.gz -> ${P}.tar.gz"

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

DEPEND=">=dev-lang/go-1.1"
RDEPEND="
	app-emulation/lxc
	net-misc/bridge-utils
	sys-apps/iproute2
	app-arch/libarchive
	net-misc/curl
	sys-fs/aufs-util
"

src_install() {
	dobin ${S}/bin/${PN}
	keepdir /var/lib/${PN}/graph
	keepdir /var/lib/${PN}/containers
	systemd_dounit "${FILESDIR}"/${PN}.service
	systemd_enable_service multi-user.target ${PN}.service
}
