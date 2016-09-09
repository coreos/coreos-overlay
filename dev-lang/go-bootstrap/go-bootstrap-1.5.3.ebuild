# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI=6

inherit coreos-go-utils

BOOTSTRAP_DIST="https://dev.gentoo.org/~williamh/dist"
SRC_URI="amd64? ( ${BOOTSTRAP_DIST}/go-linux-amd64-bootstrap.tbz )
	arm64? ( ${BOOTSTRAP_DIST}/go-linux-arm64-bootstrap.tbz )
"

KEYWORDS="-* amd64 arm64"

DESCRIPTION="Version of go compiler used for bootstrapping"
HOMEPAGE="http://www.golang.org"

LICENSE="BSD"
SLOT="0"
IUSE=""

DEPEND=""
RDEPEND=""

# Disable all QA_* checks since these are pre-built binaries.
QA_PREBUILT="usr/lib/go-bootstrap/*"

# Test data is never executed so don't check link dependencies.
REQUIRES_EXCLUDE="/usr/lib/go-bootstrap/src/debug/elf/testdata/*"

# The go language uses *.a files which are _NOT_ libraries and should not be
# stripped. The test data objects should also be left alone and unstripped.
STRIP_MASK="*.a /usr/lib/go-bootstrap/src/*"

S="${WORKDIR}"/go-$(go_os)-$(go_arch)-bootstrap

src_install() {
	dodir /usr/lib/go-bootstrap
	exeinto /usr/lib/go-bootstrap/bin
	doexe bin/*

	insopts -m0644 -p # preserve timestamps
	insinto /usr/lib/go-bootstrap
	doins -r lib pkg src
	fperms -R +x /usr/lib/go-bootstrap/pkg/tool
}
