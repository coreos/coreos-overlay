# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_COMMIT="193c41ed66d25ba76e828b0715f7f80ae88eb226"
CROS_WORKON_TREE="4e09ef237374751d99122f8225bc14a71de2c968"
CROS_WORKON_PROJECT="coreos/update_engine"
CROS_WORKON_REPO="git://github.com"

inherit toolchain-funcs cros-debug cros-workon scons-utils

DESCRIPTION="Chrome OS Update Engine"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="cros_host -delta_generator"

LIBCHROME_VERS="180609"

RDEPEND="app-arch/bzip2
	coreos-base/chromeos-ca-certificates
	coreos-base/libchrome:${LIBCHROME_VERS}[cros-debug=]
	coreos-base/libchromeos
	coreos-base/metrics
	coreos-base/verity
	dev-cpp/gflags
	dev-libs/dbus-glib
	dev-libs/glib
	dev-libs/libpcre
	dev-libs/libxml2
	dev-libs/openssl
	dev-libs/protobuf
	dev-util/bsdiff
	net-misc/curl
	sys-apps/rootdev
	sys-fs/e2fsprogs
	virtual/udev"
DEPEND="coreos-base/system_api
	dev-cpp/gmock
	dev-cpp/gtest
	cros_host? ( dev-util/scons )
	${RDEPEND}"

src_compile() {
	tc-export CC CXX AR RANLIB LD NM PKG_CONFIG
	cros-debug-add-NDEBUG
	export CCFLAGS="$CFLAGS"
	export BASE_VER=${LIBCHROME_VERS}

	escons
}

src_test() {
	UNITTESTS_BINARY=update_engine_unittests
	TARGETS="${UNITTESTS_BINARY} test_http_server delta_generator"
	escons ${TARGETS}

	if ! use x86 && ! use amd64 ; then
		einfo "Skipping tests on non-x86 platform..."
	else
		# We need to set PATH so that the `openssl` in the target
		# sysroot gets executed instead of the host one (which is
		# compiled differently). http://crosbug.com/27683
		PATH="$SYSROOT/usr/bin:$PATH" \
		"./${UNITTESTS_BINARY}" --gtest_filter='-*.RunAsRoot*' \
			&& einfo "./${UNITTESTS_BINARY} (unprivileged) succeeded" \
			|| die "./${UNITTESTS_BINARY} (unprivileged) failed, retval=$?"
		sudo LD_LIBRARY_PATH="${LD_LIBRARY_PATH}" PATH="$SYSROOT/usr/bin:$PATH" \
			"./${UNITTESTS_BINARY}" --gtest_filter='*.RunAsRoot*' \
			&& einfo "./${UNITTESTS_BINARY} (root) succeeded" \
			|| die "./${UNITTESTS_BINARY} (root) failed, retval=$?"
	fi
}

src_install() {
	dosbin update_engine
	dobin update_engine_client

	use delta_generator && dobin delta_generator

	insinto /usr/share/dbus-1/services
	doins org.chromium.UpdateEngine.service

	insinto /etc/dbus-1/system.d
	doins UpdateEngine.conf

	insinto /lib/udev/rules.d
	doins 99-gpio-dutflag.rules

	insinto /usr/include/chromeos/update_engine
	doins update_engine.dbusserver.h
	doins update_engine.dbusclient.h
}
