#
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=2
CROS_WORKON_PROJECT="coreos/systemd-rest"
CROS_WORKON_LOCALNAME="systemd-rest"
inherit toolchain-funcs cros-workon systemd

DESCRIPTION="systemd over rest"
HOMEPAGE="https://bitbucket.org/coreos/go-systemd"
SRC_URI=""

LICENSE="MIT"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE=""

DEPEND=">=dev-lang/go-1.0.2"
GOROOT="${ED}usr/$(get_libdir)/go"
GOPKG="${PN}"

src_compile() {
	GOPATH="${S}" go build ${PN}
}

src_install() {
	dosbin ${S}/systemd-rest
	systemd_dounit "${FILESDIR}"/${PN}.service
	systemd_enable_service multi-user.target ${PN}.service
}
