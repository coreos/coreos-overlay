# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_PROJECT="ibuildthecloud/systemd-docker"
CROS_WORKON_LOCALNAME="systemd-docker"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64"
else
	CROS_WORKON_COMMIT="faf1c44d3ad550b4536cea7013ad5d112d96bf9f"  #v0.1.2
	KEYWORDS="amd64"
fi

inherit cros-workon systemd

DESCRIPTION="systemd-docker"
HOMEPAGE="https://github.com/ibuildthecloud/systemd-docker"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND=">=dev-lang/go-1.2"

src_compile() {
	GOPATH=$(pwd)/Godeps/_workspace go build -o bin/systemd-docker .
}

src_install() {
	dobin ${S}/bin/systemd-docker
}
