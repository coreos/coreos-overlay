# Copyright (c) 2013 CoreOS Inc. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

inherit autotools git-2

DESCRIPTION="Build tool wrappers for using custom SYSROOTs"
HOMEPAGE="https://github.com/coreos/sysroot-wrappers"
EGIT_REPO_URI="https://github.com/coreos/sysroot-wrappers"
SRC_URI=""

LICENSE="GPL-3"
SLOT="0"
KEYWORDS="-*"
IUSE=""

src_prepare() {
	eautoreconf
}
