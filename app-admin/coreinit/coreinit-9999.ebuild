#
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Copyright (c) 2013 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=4
CROS_WORKON_PROJECT="coreos/coreinit"
CROS_WORKON_LOCALNAME="coreinit"
CROS_WORKON_REPO="git://github.com"
inherit toolchain-funcs cros-workon systemd

DESCRIPTION="coreinit"
HOMEPAGE="https://github.com/coreos/coreinit"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

DEPEND=">=dev-lang/go-1.1"

src_compile() {
	./build
}

src_install() {
	dobin ${S}/coreinit
	dobin ${S}/corectl

	systemd_dounit "${FILESDIR}"/${PN}.service
	systemd_enable_service multi-user.target ${PN}.service
}
