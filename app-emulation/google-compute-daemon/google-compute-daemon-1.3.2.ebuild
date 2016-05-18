# Copyright (c) 2014 CoreOS, Inc. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

inherit eutils

DESCRIPTION="Google Daemon for Compute Engine"
HOMEPAGE="https://github.com/GoogleCloudPlatform/compute-image-packages"
SRC_URI="https://github.com/GoogleCloudPlatform/compute-image-packages/releases/download/${PV}/google-daemon-${PV}.tar.gz"

LICENSE="MIT"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE=""

RDEPEND="dev-lang/python-oem"

S="${WORKDIR}"

src_prepare() {
	epatch "${FILESDIR}/0001-Add-users-to-docker-group-by-default.patch"
	epatch "${FILESDIR}/0002-Use-ens4v1-instead-of-eth0.patch"
}

src_install() {
	insinto "/usr/share/oem/google-compute-daemon/"
	doins -r "${S}/usr/share/google/google_daemon/."
}
