# Copyright (c) 2013 The CoreOS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="5"
CROS_WORKON_PROJECT="coreos/nss-altfiles"
CROS_WORKON_LOCALNAME="nss-altfiles"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm ~arm64 ~x86"
else
	CROS_WORKON_COMMIT="42bec47544ad80d3e39342b11ea33da05ff9133d" # v2.23.0
	KEYWORDS="amd64 arm arm64 x86"
fi

inherit cros-workon

DESCRIPTION="NSS module for data sources under /usr on for CoreOS"
HOMEPAGE="https://github.com/coreos/nss-altfiles"
SRC_URI=""

LICENSE="LGPL-2.1+"
SLOT="0"
IUSE=""

DEPEND=""
RDEPEND=""

src_configure() {
	tc-export CC
	econf \
		--datadir=/usr/share/baselayout \
		--with-module-name=usrfiles \
		--with-types=all
}

src_install() {
	dolib.so libnss_usrfiles.so.2
}
