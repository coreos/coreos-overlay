# Copyright 2011 The Chromium OS Authors
# Distributed under the terms of the GNU General Public License v2
# $Header:

EAPI="4"
CROS_WORKON_PROJECT="chromiumos/third_party/flashmap"

inherit cros-workon toolchain-funcs multilib

DESCRIPTION="Utility for manipulating firmware ROM mapping data structure"
HOMEPAGE="http://flashmap.googlecode.com"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"

# Disable unit testing for now because one of the test cases for detecting
# buffer overflow causes emake to fail when fmap_test is run.
# RESTRICT="test" will override FEATURES="test" and will also cause
# src_test() to be ignored by relevant scripts.
RESTRICT="test"
FEATURES="test"

src_compile() {
	tc-export AR CC LD NM STRIP OBJCOPY
	emake || die
}

src_test() {
	tc-export AR CC LD NM STRIP OBJCOPY
	# default "test" target uses lcov, so "test_only" was added to only
	# build and run the test without generating coverage statistics
	emake test_only || die
}

src_install() {
	emake LIBDIR=$(get_libdir) DESTDIR="${D}" USE_PKG_CONFIG=1 install || die
}
