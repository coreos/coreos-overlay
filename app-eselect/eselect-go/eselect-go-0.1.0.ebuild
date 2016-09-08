# Copyright 2016 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI=6

DESCRIPTION="Eselect module for managing multiple Go versions"
HOMEPAGE="https://github.com/coreos/eselect-go"
SRC_URI="${HOMEPAGE}/releases/download/v${PV}/${P}.tar.gz"
# Note for future releases: the tarball was generated via `make dist` and
# uploaded to GitHub so there is no need for initializing autotools here.

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm64"
IUSE="test"

DEPEND="test? ( dev-libs/glib )"
RDEPEND="app-admin/eselect
	!dev-lang/go:0"

src_configure() {
	# Go is installed to /usr/lib, not /usr/lib64
	econf --libdir=/usr/lib
}

src_install() {
	keepdir /etc/env.d/go
	default
}

pkg_postinst() {
	if has_version 'dev-lang/go'; then
		eselect go update --if-unset
	fi
}
