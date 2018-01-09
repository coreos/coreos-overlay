# Copyright (c) 2018 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the Apache 2.0 license.

EAPI=6
CROS_WORKON_PROJECT="coreos/coreos-cryptagent"
CROS_WORKON_LOCALNAME="coreos-cryptagent"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/coreos-cryptagent"
inherit coreos-go cros-workon

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="aaa8debb5419c13fe3b52135d2d5b2b4e41f9160"
	KEYWORDS="amd64 arm64"
fi

DESCRIPTION="coreos-cryptagent"
HOMEPAGE="https://github.com/coreos/coreos-cryptagent"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

src_compile() {
        go_build "${COREOS_GO_PACKAGE}"
}

src_install() {
        exeinto /usr/lib/coreos
        doexe "${GOBIN}"/coreos-cryptagent
}
