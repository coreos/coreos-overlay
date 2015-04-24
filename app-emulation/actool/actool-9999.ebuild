# Copyright (c) 2015 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="appc/spec"
CROS_WORKON_REPO="git://github.com"
# name of directory git repo is checked out into by manifest
CROS_WORKON_LOCALNAME="appc-spec"
COREOS_GO_PACKAGE="github.com/appc/spec"
inherit coreos-go cros-workon

if [[ "${PV}" == 9999 ]]; then
    KEYWORDS="~amd64"
else
    CROS_WORKON_COMMIT="9a448f73b7fa765a60eade4bcca41e18bfe613aa" # v0.5.1
    KEYWORDS="amd64"
fi

DESCRIPTION="App Container builder and validator"
HOMEPAGE="https://github.com/appc/spec"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND=">=dev-lang/go-1.3"

src_compile() {
	go_build "${COREOS_GO_PACKAGE}/actool"
}

src_install() {
	dobin "${WORKDIR}/gopath/bin/actool"
}
