# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/updateservicectl"
CROS_WORKON_LOCALNAME="updateservicectl"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/updateservicectl"
inherit cros-workon coreos-go

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="f9a7976a09697a15c41a1affb4866790ef714332"  # tag v1.3.0
	KEYWORDS="amd64 arm64 ppc64"
fi

DESCRIPTION="CoreUpdate Management CLI"
HOMEPAGE="https://github.com/coreos/updateservicectl"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

RDEPEND="!app-admin/updatectl"

src_prepare() {
	coreos-go_src_prepare
	GOPATH+=":${S}/Godeps/_workspace"
}
