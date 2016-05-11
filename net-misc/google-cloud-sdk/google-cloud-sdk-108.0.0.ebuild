# Copyright 1999-2016 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI=6

PYTHON_COMPAT=( python2_7 )

inherit eutils python-r1

DESCRIPTION="Command line tool for interacting with Google Compute Engine"
HOMEPAGE="https://cloud.google.com/sdk/#linux"
SRC_URI="https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/${P}-linux-x86_64.tar.gz"

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

S="${WORKDIR}/${PN}"

DEPEND="${PYTHON_DEPS}"
RDEPEND="${DEPEND}"

src_prepare() {
	epatch "${FILESDIR}/0001-Hardcode-the-root-directory.patch"

	default
}

src_install() {
	dobin "${S}/bin/gcloud"

	insinto "/usr/lib/${PN}"
	doins -r "${S}/lib"
}
