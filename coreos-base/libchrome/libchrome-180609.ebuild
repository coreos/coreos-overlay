# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

# Note: the ${PV} should represent the overall svn rev number of the
# chromium tree that we're extracting from rather than the svn rev of
# the last change actually made to the base subdir.  This way packages
# from other locations (like libchrome_crypto) can be coordinated.

# XXX: This hits svn rev 180557 instead of rev 180609, but
#      that is correct.  See above note.

EAPI="4"
CROS_WORKON_COMMIT="94b9b5d64fa557377ab1e3a5e3bd6cca7d0b73d8"
CROS_WORKON_PROJECT="chromium/src/base"
CROS_WORKON_GIT_SUFFIX="-${PV}"

inherit cros-workon cros-debug toolchain-funcs scons-utils

DESCRIPTION="Chrome base/ library extracted for use on Chrome OS"
HOMEPAGE="http://dev.chromium.org/chromium-os/packages/libchrome"
SRC_URI=""

LICENSE="BSD"
SLOT="${PV}"
KEYWORDS="amd64 arm x86"
IUSE="cros_host"

RDEPEND="dev-libs/glib
	dev-libs/libevent"
DEPEND="${RDEPEND}
	dev-cpp/gtest
	cros_host? ( dev-util/scons )"

src_prepare() {
	ln -s "${S}" "${WORKDIR}/base" &> /dev/null

	mkdir -p "${WORKDIR}/build"
	cp -p "${FILESDIR}/build_config.h-${SLOT}" "${WORKDIR}/build/build_config.h" || die

	cp -p "${FILESDIR}/SConstruct-${SLOT}" "${S}/SConstruct" || die
	epatch "${FILESDIR}"/gtest_include_path_fixup.patch

	epatch "${FILESDIR}"/base-125070-no-X.patch
	epatch "${FILESDIR}"/base-125070-x32.patch
}

src_configure() {
	tc-export CC CXX AR RANLIB LD NM PKG_CONFIG
	cros-debug-add-NDEBUG
	export CCFLAGS="$CFLAGS"

	mkdir -p third_party/libevent
	echo '#include <event.h>' > third_party/libevent/event.h
}

src_compile() {
	BASE_VER=${SLOT} escons -k
}

src_install() {
	dolib.so libbase*-${SLOT}.so

	local d header_dirs=(
		third_party/icu
		third_party/nspr
		third_party/valgrind
		third_party/dynamic_annotations
		.
		debug
		files
		json
		memory
		metrics
		posix
		strings
		synchronization
		threading
	)
	for d in "${header_dirs[@]}" ; do
		insinto /usr/include/base-${SLOT}/base/${d}
		doins ${d}/*.h
	done

	insinto /usr/include/base-${SLOT}/build
	doins "${WORKDIR}"/build/build_config.h

	insinto /usr/$(get_libdir)/pkgconfig
	doins libchrome-${SLOT}.pc
}
