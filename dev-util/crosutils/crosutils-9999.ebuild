# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=6
CROS_WORKON_PROJECT="chromiumos/platform/crosutils"
CROS_WORKON_LOCALNAME="../scripts/"

inherit python-utils-r1 cros-workon

DESCRIPTION="Chromium OS build utilities"
HOMEPAGE="http://www.chromium.org/chromium-os"

LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64"
IUSE=""

src_configure() {
	find . -type l -exec rm {} \; &&
	rm -fr WATCHLISTS inherit-review-settings-ok lib/shflags ||
		die "Couldn't clean directory."
}

src_install() {
	# Install package files
	exeinto /usr/lib/crosutils
	doexe * || die "Could not install shared files."

	insinto "$(python_get_sitedir python2_7 PYTHON)"
	doins lib/*.py || die "Could not install python files."
	rm -f lib/*.py

	# Install libraries
	insinto /usr/lib/crosutils/lib
	doins lib/* || die "Could not install library files"
}
