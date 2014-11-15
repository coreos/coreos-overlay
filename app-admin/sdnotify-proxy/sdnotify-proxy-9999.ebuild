# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_PROJECT="coreos/sdnotify-proxy"
CROS_WORKON_LOCALNAME="sdnotify-proxy"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64"
else
	CROS_WORKON_COMMIT="f516129af110fd4a51bd5ea2654f4dc8dd02e5c4"
	KEYWORDS="amd64"
fi

inherit cros-workon

DESCRIPTION="fleet"
HOMEPAGE="https://github.com/coreos/sdnotify-proxy"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND=">=dev-lang/go-1.3"

src_compile() {
	./build
}

src_install() {
	exeinto /usr/libexec
	doexe ${S}/bin/sdnotify-proxy
}
