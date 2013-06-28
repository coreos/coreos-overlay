# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5
PYTHON_COMPAT=( python{2_5,2_6,2_7} )

inherit distutils-r1

MY_PN="SocksiPy-branch"
MY_P="${MY_PN}-${PV}"

DESCRIPTION="Google API Client Library for Python"
HOMEPAGE="http://code.google.com/p/socksipy-branch/"
SRC_URI="http://socksipy-branch.googlecode.com/files/${MY_P}.zip"

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

RDEPEND="${PYTHON_DEPS}"
DEPEND="${RDEPEND}"

S="${WORKDIR}/${MY_P}"

DOCS=( BUGS README )
