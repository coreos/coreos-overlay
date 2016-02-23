# Copyright (c) 2014 CoreOS, Inc. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

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
	epatch "${FILESDIR}"/0001-fix-google-startup-scripts-use-GOOGLE_STARTUP_SCRIPT.patch
}

src_install() {
	mkdir -p ${D}/usr/share/oem/google-startup-scripts/usr/share/google/
	cp -Ra ${WORKDIR}/usr/share/google/. ${D}/usr/share/oem/google-startup-scripts/ || die
	# We don't install python or gsutil so skip this
	rm -R ${D}/usr/share/oem/google-startup-scripts/boto || die
}
