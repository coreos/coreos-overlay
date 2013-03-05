# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_COMMIT="6e8f24e5f10aeba2feb9d72a49f276fc0f2ed7de"
CROS_WORKON_TREE="4677180b651268e80a28763a89399281ec778d46"
CROS_WORKON_PROJECT="chromiumos/platform/libchromeos"

LIBCHROME_VERS=( 180609 )

inherit toolchain-funcs cros-debug cros-workon scons-utils

DESCRIPTION="Chrome OS base library."
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="cros_host test"

LIBCHROME_DEPEND=$(
	printf \
		'coreos-base/libchrome:%s[cros-debug=] ' \
		${LIBCHROME_VERS[@]}
)
RDEPEND="${LIBCHROME_DEPEND}
	dev-libs/dbus-c++
	dev-libs/dbus-glib
	dev-libs/openssl
	dev-libs/protobuf"

DEPEND="${RDEPEND}
	coreos-base/protofiles
	test? ( dev-cpp/gtest )
	cros_host? ( dev-util/scons )"

cr_scons() {
	local v=$1; shift
	BASE_VER=${v} escons -C ${v} -Y "${S}" "$@"
}

src_compile() {
	tc-export CC CXX AR RANLIB LD NM PKG_CONFIG
	cros-debug-add-NDEBUG
	export CCFLAGS="$CFLAGS"

	local v
	mkdir -p ${LIBCHROME_VERS[@]}
	for v in ${LIBCHROME_VERS[@]} ; do
		cr_scons ${v} libchromeos-${v}.{pc,so} libpolicy-${v}.so
	done
}

src_test() {
	local v
	for v in ${LIBCHROME_VERS[@]} ; do
		cr_scons ${v} unittests libpolicy_unittest
		if ! use x86 && ! use amd64 ; then
			ewarn "Skipping unit tests on non-x86 platform"
		else
			./${v}/unittests || die "libchromeos-${v} failed"
			./${v}/libpolicy_unittest || die "libpolicy_unittest-${v} failed"
		fi
	done
}

src_install() {
	local v
	insinto /usr/$(get_libdir)/pkgconfig
	for v in ${LIBCHROME_VERS[@]} ; do
		dolib.so ${v}/lib{chromeos,policy}*-${v}.so
		doins ${v}/libchromeos-${v}.pc
	done

	insinto /usr/include/chromeos
	doins chromeos/*.h

	insinto /usr/include/chromeos/dbus
	doins chromeos/dbus/*.h

	insinto /usr/include/chromeos/glib
	doins chromeos/glib/*.h

	insinto /usr/include/policy
	doins chromeos/policy/*.h
}
