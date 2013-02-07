# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_COMMIT="f91b983c430f2e7ce125fda8704d094616d24a0c"
CROS_WORKON_TREE="e18118e277349f02968917bc3eddb4dc39722e05"
CROS_WORKON_PROJECT="chromiumos/platform/chromiumos-assets"

inherit cros-workon toolchain-funcs

DESCRIPTION="Chromium OS-specific assets"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""
LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

PDEPEND=">chromeos-base/chromeos-assets-0.0.1-r47"
DEPEND=""
RDEPEND=""

CROS_WORKON_LOCALNAME="chromiumos-assets"

src_install() {
	insinto /usr/share/chromeos-assets/images
	doins -r "${S}"/images/*

	insinto /usr/share/chromeos-assets/images_100_percent
	doins -r "${S}"/images_100_percent/*

	insinto /usr/share/chromeos-assets/images_200_percent
	doins -r "${S}"/images_200_percent/*

	insinto /usr/share/chromeos-assets/screensavers
	doins -r "${S}"/screensavers/*
}
