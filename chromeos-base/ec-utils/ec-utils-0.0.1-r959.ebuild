# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_COMMIT="6e29f8091d9da763cd39b7ea61eefb5c35b94606"
CROS_WORKON_TREE="7312bc4c61477f0041b02dbca07a3029707e9d46"
CROS_WORKON_PROJECT="chromiumos/platform/ec"
CROS_WORKON_LOCALNAME="../platform/ec"

inherit cros-workon toolchain-funcs cros-board

DESCRIPTION="Chrome OS EC Utility"

HOMEPAGE="http://www.chromium.org/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

set_board() {
	export BOARD=$(get_current_board_with_variant)
	if [[ ! -d board/${BOARD} ]] ; then
		ewarn "${BOARD} does not use Chrome EC. Setting BOARD=bds."
		BOARD=bds
	fi
}

src_compile() {
	tc-export AR CC RANLIB
	# In platform/ec Makefile, it uses "CC" to specify target chipset and
	# "HOSTCC" to compile the utility program because it assumes developers
	# want to run the utility from same host (build machine).
	# In this ebuild file, we only build utility
	# and we may want to build it so it can
	# be executed on target devices (i.e., arm/x86/amd64), not the build
	# host (BUILDCC, amd64). So we need to override HOSTCC by target "CC".
	export HOSTCC="$CC"
	set_board
	emake utils
}

src_install() {
	set_board
	dosbin "build/$BOARD/util/ectool"
	if [[ -d board/${BOARD}/userspace/etc/init ]] ; then
		insinto /etc/init
		doins board/${BOARD}/userspace/etc/init/*.conf
	fi
	if [[ -d board/${BOARD}/userspace/usr/share/ec ]] ; then
		insinto /usr/share/ec
		doins board/${BOARD}/userspace/usr/share/ec/*
	fi
}
