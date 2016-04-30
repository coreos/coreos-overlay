# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/sdnotify-proxy"
CROS_WORKON_LOCALNAME="sdnotify-proxy"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/sdnotify-proxy"
inherit coreos-go cros-workon

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="f516129af110fd4a51bd5ea2654f4dc8dd02e5c4"
	KEYWORDS="amd64 arm64 ppc64"
fi

DESCRIPTION="sdnotify-proxy"
HOMEPAGE="https://github.com/coreos/sdnotify-proxy"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

src_install() {
	# Put sdnotify-proxy into its well-know location.
	exeinto /usr/libexec
	doexe ${GOBIN}/sdnotify-proxy
}
