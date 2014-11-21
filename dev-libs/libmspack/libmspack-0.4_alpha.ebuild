# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

MY_P="${P/_/}"

DESCRIPTION="A library for Microsoft compression formats"
HOMEPAGE="http://www.cabextract.org.uk/libmspack/"
SRC_URI="http://www.cabextract.org.uk/libmspack/${MY_P}.tar.gz"

LICENSE="LGPL-2"
SLOT="0"
KEYWORDS="amd64"
IUSE="static-libs"

S="${WORKDIR}/${MY_P}"

src_configure() {
	econf \
		--prefix=/usr/share/oem \
		$(use_enable static-libs static)
}
