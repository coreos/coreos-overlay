# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_COMMIT="2a813cecf1b7357b1a2faf0f9e5bbb73ba9276b6"
CROS_WORKON_PROJECT="chromium/src/courgette"

inherit cros-workon cros-debug toolchain-funcs scons-utils

DESCRIPTION="Chrome courgette/ library extracted for use on Chrome OS"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

LIBCHROME_VERS="125070"

RDEPEND="chromeos-base/libchrome:${LIBCHROME_VERS}[cros-debug=]"
DEPEND="${RDEPEND}"

src_prepare() {
	cp "${FILESDIR}"/SConstruct "${S}"/ || die
}

src_compile() {
	tc-export CC CXX AR RANLIB LD NM PKG_CONFIG
	cros-debug-add-NDEBUG
	export BASE_VER=${LIBCHROME_VERS}

	escons
}

src_install() {
	dolib.a libcourgette.a

	insinto /usr/include/courgette
	doins courgette.h
}
