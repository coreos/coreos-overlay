# Copyright 2013 The CoreOS Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=5

inherit systemd git

DESCRIPTION="Docker container management"
HOMEPAGE="http://docker.io"

EGIT_REPO_SERVER="https://github.com"
EGIT_REPO_URI="${EGIT_REPO_SERVER}/philips/docker.git"
EGIT_BRANCH="fd045c1038690372a656d45929bfb5e54975a229" # 0.6.1 with Brandon's build system


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

src_compile() {
	./vendor.sh || die
	./hack/release/make-without-docker.sh || die
}

src_install() {
	dobin ${S}/bin/${PN}
	keepdir /var/lib/${PN}/graph
	keepdir /var/lib/${PN}/containers
	systemd_dounit "${FILESDIR}"/${PN}.service || die
	systemd_enable_service multi-user.target ${PN}.service || die
}
