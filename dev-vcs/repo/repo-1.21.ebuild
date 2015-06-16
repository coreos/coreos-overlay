# Copyright (c) 2014 CoreOS, Inc. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

PYTHON_COMPAT=( python2_7 )

inherit python-r1

DESCRIPTION="Android's multiple git repository management tool."
HOMEPAGE="https://code.google.com/p/git-repo/"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64 arm64"
IUSE=""

DEPEND="${PYTHON_DEPS}"
RDEPEND="${DEPEND}
	>=dev-vcs/git-1.7.2
	app-crypt/gnupg"

S="${WORKDIR}"

src_install() {
	python_foreach_impl python_newscript "${FILESDIR}/${P}" repo
}
