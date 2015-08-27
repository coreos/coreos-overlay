# Copyright (c) 2015 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/coreos-metadata"
CROS_WORKON_LOCALNAME="coreos-metadata"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/coreos-metadata"
inherit coreos-go cros-workon systemd

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64"
else
	CROS_WORKON_COMMIT="416392f69e60bcd96507cb626a9d3fe380f0af1e" # v0.1.0
	KEYWORDS="amd64"
fi

DESCRIPTION="A simple cloud-provider metadata agent"
HOMEPAGE="https://github.com/coreos/coreos-metadata"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

src_install() {
	dobin "${GOBIN}/${PN}"
	systemd_dounit "${FILESDIR}/coreos-metadata.service"
}
