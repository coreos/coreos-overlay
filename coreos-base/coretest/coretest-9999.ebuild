# Copyright (c) 2014 The CoreOS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/coretest"
CROS_WORKON_LOCALNAME="coretest"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/coretest"
inherit cros-workon coreos-go

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64 ~x86"
else
	CROS_WORKON_COMMIT="991faaf28eb21f185fed0708b526849a8bc128e6"
	KEYWORDS="amd64 arm64 x86 ppc64"
fi

DESCRIPTION="Sanity tests for CoreOS"
HOMEPAGE="https://github.com/coreos/coretest"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

src_prepare() {
	coreos-go_src_prepare
	GOPATH+=":${S}/third_party"
}

src_install() {
	go test -i -c -o "${GOBIN}/coretest" "github.com/coreos/coretest" \
		|| die "go test failed"
	dobin "${GOBIN}/coretest"
}
