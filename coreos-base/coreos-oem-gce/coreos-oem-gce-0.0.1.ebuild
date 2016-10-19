# Copyright (c) 2016 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2

DESCRIPTION="CoreOS OEM suite for Google Compute Engine (meta package)"
HOMEPAGE=""

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

RDEPEND="
	app-emulation/google-compute-engine
	sys-libs/glibc
	sys-libs/nss-usrfiles
"
