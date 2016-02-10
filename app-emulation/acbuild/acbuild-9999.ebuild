# Copyright (c) 2015 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="appc/acbuild"
CROS_WORKON_REPO="git://github.com"
CROS_WORKON_LOCALNAME="appc-acbuild"
COREOS_GO_PACKAGE="github.com/appc/acbuild"
inherit coreos-go toolchain-funcs cros-workon

if [[ "${PV}" == 9999 ]]; then
    KEYWORDS="~amd64 ~arm64"
else
    CROS_WORKON_COMMIT="e49e7de27443374a5356b7ccd9d9d7691fb5089f" # v0.2.2
    KEYWORDS="amd64 arm64"
fi

DESCRIPTION="A build tool for ACIs"
HOMEPAGE="https://github.com/appc/acbuild"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND=">=dev-lang/go-1.4.3"
RDEPEND="sys-apps/kmod
	app-crypt/gnupg
	sys-apps/systemd"

src_compile(){
	if [[ "${PV}" == 9999 ]]; then
		# set semver
		local v
		v=$(git describe --long --dirty) || die
		v=${v#v}
		v=${v/-/+}
		GO_LDFLAGS="-X ${COREOS_GO_PACKAGE}/lib.Version ${v}"
	else
		GO_LDFLAGS="-X ${COREOS_GO_PACKAGE}/lib.Version ${PV}"
	fi

	go_build "${COREOS_GO_PACKAGE}/acbuild"
}

src_install(){
	dobin "${WORKDIR}/gopath/bin/acbuild"
}

