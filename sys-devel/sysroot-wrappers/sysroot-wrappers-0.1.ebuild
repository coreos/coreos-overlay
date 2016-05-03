# Copyright (c) 2013 CoreOS Inc. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

DESCRIPTION="Build tool wrappers for using custom SYSROOTs"
HOMEPAGE="https://github.com/coreos/sysroot-wrappers"
SRC_URI="https://github.com/coreos/${PN}/releases/download/v${PV}/${P}.tar.gz"

LICENSE="GPL-3"
SLOT="0"
KEYWORDS="amd64 ppc64"
IUSE=""

# Probably can be reduced in later versions but
# this is what this release is set to expect.
DEPEND=">=sys-devel/autoconf-2.69
		>=sys-devel/automake-1.12"
