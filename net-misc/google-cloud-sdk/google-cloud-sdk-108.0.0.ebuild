# Copyright 1999-2016 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI=6

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

src_install() {
	into "/usr/share/oem/google-cloud-sdk/"
	dobin "${S}/bin/gcloud"
	dobin "${S}/bin/gsutil"

	insinto "/usr/share/oem/google-cloud-sdk/"
	doins -r "${S}/lib"
}
