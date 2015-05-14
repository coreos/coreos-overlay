# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_PROJECT="coreos/vboot_reference"
CROS_WORKON_REPO="git://github.com"
CROS_WORKON_LOCALDIR="src/platform"
AUTOTOOLS_AUTORECONF=1

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm ~x86"
else
	CROS_WORKON_COMMIT="8881575d1372b860744afe57b52a153a03c80c6a"
	KEYWORDS="amd64 arm x86"
fi

inherit autotools-utils cros-workon

DESCRIPTION="CoreOS Disk Utilities (e.g. cgpt)"
LICENSE="BSD"
SLOT="0"
IUSE=""

RDEPEND="sys-apps/util-linux"
DEPEND="${RDEPEND}"
