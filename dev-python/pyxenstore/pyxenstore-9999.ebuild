# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=4
PYTHON_DEPEND="2"
SUPPORT_PYTHON_ABIS="1"
RESTRICT_PYTHON_ABIS=""

inherit distutils bzr

EBZR_REPO_URI="https://code.launchpad.net/~cbehrens/pyxenstore/trunk"

DESCRIPTION="Provides Python interfaces for Xen's XenStore."
HOMEPAGE="https://launchpad.net/pyxenstore"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE=""

DEPEND="
	app-emulation/xen-tools
	"
RDEPEND="${DEPEND}"
