# Copyright (c) 2014 CoreOS, Inc. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

inherit eutils

DESCRIPTION="Google Startup Scripts for Compute Engine"
HOMEPAGE="https://github.com/GoogleCloudPlatform/compute-image-packages"
SRC_URI="https://github.com/GoogleCloudPlatform/compute-image-packages/releases/download/${PV}/${P}.tar.gz"

LICENSE="MIT"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE=""

RDEPEND="dev-lang/python-oem"

S="${WORKDIR}"

src_prepare() {
	epatch "${FILESDIR}/0001-Allow-location-of-startup-scripts-to-be-overridden.patch"
}

src_install() {
	exeinto "/usr/share/oem/google-startup-scripts/"
	doexe "${S}"/usr/share/google/*
}
