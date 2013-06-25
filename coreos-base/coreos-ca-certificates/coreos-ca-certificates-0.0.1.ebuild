# Copyright (c) 2013 CoreOS Inc. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2

DESCRIPTION="CoreOS restricted set of certificates."
HOMEPAGE="http://github.com/coreos/"
SRC_URI=""
LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"

src_install() {
	CA_CERT_DIR=/usr/share/coreos-ca-certificates
	insinto "${CA_CERT_DIR}"
	doins "${FILESDIR}"/*.pem
	c_rehash "${D}/${CA_CERT_DIR}"
}
