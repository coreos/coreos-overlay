# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_COMMIT="7cbe18c6f1b8446d486a501a94297e6b04128b6d"
CROS_WORKON_TREE="b5ef87a2635b6cbba9816bf32c256b3cdec80cef"
CROS_WORKON_PROJECT="chromiumos/platform/bootstat"
inherit cros-workon

DESCRIPTION="Chrome OS Boot Time Statistics Utilities"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""
LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 x86 arm"
IUSE=""

RDEPEND=""

DEPEND="dev-cpp/gtest"

src_compile() {
	tc-export CC CXX AR PKG_CONFIG
	emake || die "bootstat compile failed."
}

src_test() {
	tc-export CC CXX AR PKG_CONFIG
	emake tests || die "could not build tests"
	if ! use x86 && ! use amd64 ; then
		echo Skipping unit tests on non-x86 platform
	else
		for test in ./*_test; do
			"${test}" ${GTEST_ARGS} || die "${test} failed"
		done
	fi
}

src_install() {
	into /
	dosbin bootstat || die
	dosbin bootstat_get_last || die
	dobin bootstat_summary || die

	into /usr
	dolib.a libbootstat.a || die

	insinto /usr/include/metrics
	doins bootstat.h || die
}
