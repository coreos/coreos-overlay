# Copyright (c) 2015 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="appc/spec"
CROS_WORKON_REPO="git://github.com"
CROS_WORKON_LOCALNAME="appc-spec"
COREOS_GO_PACKAGE="github.com/appc/spec"
inherit coreos-go cros-workon

if [[ "${PV}" == 9999 ]]; then
    KEYWORDS="~amd64 ~arm64"
else
    CROS_WORKON_COMMIT="cbe99b7160b1397bf89f9c8bb1418f69c9424049" # v0.8.11
    KEYWORDS="amd64 arm64"
fi

DESCRIPTION="App Container builder and validator"
HOMEPAGE="https://github.com/appc/spec"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

src_compile() {
	go_build "${COREOS_GO_PACKAGE}/actool"
}

src_install() {
	dobin "${WORKDIR}/gopath/bin/actool"
}
