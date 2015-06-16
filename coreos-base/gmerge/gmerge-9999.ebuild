# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_PROJECT="coreos/dev-util"
CROS_WORKON_REPO="git://github.com"
CROS_WORKON_LOCALNAME="dev"
CROS_WORKON_LOCALDIR="src/platform"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm ~arm64 ~x86"
else
	CROS_WORKON_COMMIT="66e206c20f22e002bbdaed1982b08b18a9b7f730"
	KEYWORDS="amd64 arm arm64 x86"
fi

inherit cros-workon

DESCRIPTION="emerge utilities for CoreOS developer images"
HOMEPAGE="https://github.com/coreos/dev-util/"

LICENSE="GPL-2"
SLOT="0"
IUSE=""

RDEPEND="dev-lang/python
	sys-apps/portage"
DEPEND=""

src_compile() {
	echo "Nothing to compile"
}

src_install() {
	dobin gmerge
	dobin emerge-gitclone
}
