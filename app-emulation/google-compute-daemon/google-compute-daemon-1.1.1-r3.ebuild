# Copyright (c) 2014 CoreOS, Inc. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

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
	epatch "${FILESDIR}"/0001-Fixes-authorized_keys-file-permissions.patch
	epatch "${FILESDIR}"/0001-fix-google-daemon-use-for-the-passwd-not.patch
	epatch "${FILESDIR}"/0001-hack-address_manager-use-CoreOS-names-and-locations.patch
	epatch "${FILESDIR}"/0001-feat-accounts-add-users-to-the-sudo-and-docker-group.patch
}

src_install() {
	mkdir -p ${D}/usr/share/oem/google-compute-daemon/
	cp -Ra ${WORKDIR}/usr/share/google/google_daemon/. ${D}/usr/share/oem/google-compute-daemon/ || die
}
