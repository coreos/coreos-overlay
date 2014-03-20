# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_PROJECT="coreos/dev-util"
CROS_WORKON_REPO="git://github.com"
CROS_WORKON_LOCALNAME="dev"
CROS_WORKON_LOCALDIR="src/platform"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm ~x86"
else
	CROS_WORKON_COMMIT="4b158cdacf0fa4c16933e401dd11962b3c6d9838"
	KEYWORDS="amd64 arm x86"
fi

inherit cros-workon

DESCRIPTION="A util for installing packages using the CrOS dev server"
HOMEPAGE="http://www.chromium.org/"

LICENSE="GPL-2"
SLOT="0"
IUSE=""

RDEPEND="app-shells/bash
	dev-lang/python
	dev-util/shflags
	sys-apps/portage"
DEPEND="${RDEPEND}"

src_install() {
	into /usr
	dobin gmerge stateful_update crdev
}
