# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_COMMIT="c9880282b115a0127c948d16f4ab3c6cadcbfcd2"
CROS_WORKON_TREE="8ec533cfbe40ad80859408561a3623f8b24b909b"
CROS_WORKON_PROJECT="chromiumos/third_party/rootdev"

inherit toolchain-funcs cros-workon

DESCRIPTION="Chrome OS root block device tool/library"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""
LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 x86 arm arm64 ppc64"
IUSE=""

src_compile() {
	tc-getCC
	emake || die
}

src_install() {
	dodir /usr/bin
	exeinto /usr/bin
	doexe ${S}/rootdev

	dodir /usr/lib
	dolib.so librootdev.so*

	dodir /usr/include/rootdev
	insinto /usr/include/rootdev
	doins rootdev.h
}
