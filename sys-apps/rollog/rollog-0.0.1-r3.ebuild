# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_COMMIT="2107f76d09ee7d41fd20367af4620b46a6443af4"
CROS_WORKON_TREE="f8747f25994455c144df5a4e24cedbbe09a16d7c"
CROS_WORKON_PROJECT="chromiumos/platform/rollog"
CROS_WORKON_LOCALNAME="../platform/rollog"
CROS_WORKON_OUTOFTREE_BUILD=1

inherit cros-workon

DESCRIPTION="Utility for implementing rolling logs for bug regression"
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
	dobin "${OUT}"/rollog
}
