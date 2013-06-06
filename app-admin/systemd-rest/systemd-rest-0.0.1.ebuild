#
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=2
CROS_WORKON_PROJECT="coreos/systemd-rest"
CROS_WORKON_LOCALNAME="systemd-rest"
CROS_WORKON_REPO="git://github.com"
CROS_WORKON_COMMIT="d1da3004cdbe19afcc7a81d274085efd0b73ba64"
inherit toolchain-funcs cros-workon systemd

DESCRIPTION="systemd over rest"
HOMEPAGE="https://github.com/coreos/systemd-rest"
SRC_URI=""

LICENSE="MIT"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

DEPEND=">=dev-lang/go-1.0.2"
GOROOT="${ED}usr/$(get_libdir)/go"
GOPKG="${PN}"

src_compile() {
	export GOPATH="${S}"
	go get
	go build -o ${PN} || die
}

src_install() {
	dosbin ${S}/systemd-rest
	keepdir /var/lib/${PN}
	systemd_dounit "${FILESDIR}"/${PN}.service
	systemd_enable_service multi-user.target ${PN}.service
}
