# Copyright (c) 2013 CoreOS Inc. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

DESCRIPTION="CoreOS auto update signing keys."
HOMEPAGE="http://github.com/coreos/"
SRC_URI=""
LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm arm64 x86 ppc64"
IUSE="cros_host official"

# Do not enable offical and SDK at the same time,
# the SDK only needs developer keys.
REQUIRED_USE="?? ( cros_host official )"

# No source to unpack
S="${WORKDIR}"

src_install() {
	insinto /usr/share/update_engine
	if use official; then
		newins "${FILESDIR}/official-v2.pub.pem" update-payload-key.pub.pem
	else
		newins "${FILESDIR}/developer-v1.key.pem" update-payload-key.key.pem
		newins "${FILESDIR}/developer-v1.pub.pem" update-payload-key.pub.pem
	fi
}
