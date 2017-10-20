# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/dev-util"
CROS_WORKON_REPO="git://github.com"
CROS_WORKON_LOCALNAME="dev"
CROS_WORKON_LOCALDIR="src/platform"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm ~arm64 ~x86"
else
	CROS_WORKON_COMMIT="7b9092b9f6d93f00527591a7c393e2be3099278d"
	KEYWORDS="amd64 arm arm64 x86"
fi

PYTHON_COMPAT=( python2_7 )

inherit cros-workon python-single-r1

DESCRIPTION="emerge utilities for CoreOS developer images"
HOMEPAGE="https://github.com/coreos/dev-util/"

LICENSE="GPL-2"
SLOT="0"
IUSE=""

RDEPEND="sys-apps/portage"

src_compile() {
	echo "Nothing to compile"
}

src_install() {
	python_doscript gmerge
	python_doscript emerge-gitclone
}
