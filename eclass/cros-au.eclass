# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

#
# Original Author: The Chromium OS Authors <chromium-os-dev@chromium.org>
# Purpose: Library for handling packages that are part of Auto Update.
#

inherit flag-o-matic

# Some boards started out 32bit (user/kernel) and then migrated to 64bit
# (user/kernel).  Since we need to auto-update (AU) from the 32bit to
# 64bit, and the old 32bit kernels can't execte 64bit code, we need to
# always build the AU code as 32bit.
#
# Setup the build env to create 32bit objects.
board_setup_32bit_au_env()
{
	[[ $# -eq 0 ]] || die "${FUNCNAME}: takes no arguments"

	__AU_OLD_ARCH=${ARCH}
	__AU_OLD_ABI=${ABI}
	__AU_OLD_LIBDIR_x86=${LIBDIR_x86}
	export ARCH=x86 ABI=x86 LIBDIR_x86="lib"
	__AU_OLD_CHOST=${CHOST}
	export CHOST=i686-pc-linux-gnu
	__AU_OLD_SYSROOT=${SYSROOT}
	export SYSROOT=/usr/${CHOST}
	append-ldflags -L"${__AU_OLD_SYSROOT}"/usr/lib
	append-cxxflags -isystem "${__AU_OLD_SYSROOT}"/usr/include
}

# undo what we did in the above function
board_teardown_32bit_au_env()
{
	[[ $# -eq 0 ]] || die "${FUNCNAME}: takes no arguments"
	[ -z "${__AU_OLD_SYSROOT}" ] && \
		die "board_setup_32bit_au_env must be called first"

	filter-ldflags -L"${__AU_OLD_SYSROOT}"/usr/lib
	filter-flags -isystem "${__AU_OLD_SYSROOT}"/usr/include
	export SYSROOT=${__AU_OLD_SYSROOT}
	export CHOST=${__AU_OLD_CHOST}
	export LIBDIR_x86=${__AU_OLD_LIBDIR_x86}
	export ABI=${__AU_OLD_ABI}
	export ARCH=${__AU_OLD_ARCH}
}
