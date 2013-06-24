# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

EAPI="2"

inherit confutils

DESCRIPTION="NVRAM image for the brcmfmac driver"
HOMEPAGE="http://src.chromium.org"
SRC_URI=""
LICENSE="GPL-2"
SLOT="0"
KEYWORDS="arm"
NVRAM_USE="awnh610 awnh930"
IUSE=${NVRAM_USE}

pkg_setup() {
	confutils_init
	confutils_require_one ${NVRAM_USE}
}

src_install() {
	insinto /lib/firmware/brcm
	if use awnh610 ; then
		newins "${FILESDIR}"/bcm4329-fullmac-4.txt-awnh610 bcm4329-fullmac-4.txt || die
	elif use awnh930 ; then
		newins "${FILESDIR}"/bcm4329-fullmac-4.txt-awnh930 bcm4329-fullmac-4.txt || die
	fi
}
