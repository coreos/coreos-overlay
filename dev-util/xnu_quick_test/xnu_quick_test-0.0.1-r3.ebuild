# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_COMMIT="52004833d705bf13d177d2dc4781fc89edb90622"
CROS_WORKON_TREE="275bcb3a81b419a073305e0ab91feca9c18ff48e"
CROS_WORKON_PROJECT="chromiumos/platform/xnu_quick_test"
CROS_WORKON_LOCALNAME="../platform/xnu_quick_test"
CROS_WORKON_OUTOFTREE_BUILD=1

inherit cros-workon

DESCRIPTION="Simple kernel regression test suite"
HOMEPAGE="http://www.chromium.org/"

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

src_prepare() {
	cros-workon_src_prepare
}

src_configure() {
	cros-workon_src_configure
}

src_compile() {
	cros-workon_src_compile
}

src_install() {
	echo "Not convinced where this should be installed yet."
}
