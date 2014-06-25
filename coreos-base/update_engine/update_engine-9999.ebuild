# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_PROJECT="coreos/update_engine"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm ~x86"
else
	CROS_WORKON_COMMIT="05b5fc35063a2c12b0e7288fbb859585b2bb4959"
	KEYWORDS="amd64 arm x86"
fi

inherit toolchain-funcs cros-debug cros-workon scons-utils systemd

DESCRIPTION="Chrome OS Update Engine"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
IUSE="cros_host -delta_generator symlink-usr"

LIBCHROME_VERS="180609"

RDEPEND="!coreos-base/coreos-installer
	app-arch/bzip2
	coreos-base/coreos-au-key
	coreos-base/libchrome:${LIBCHROME_VERS}[cros-debug=]
	coreos-base/libchromeos
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
	dosbin systemd/update_engine_stub
	dobin update_engine_client

	dosbin coreos-postinst
	dosbin coreos-setgoodroot
	if use symlink-usr; then
		dosym sbin/coreos-postinst /usr/postinst
	else
		dosym usr/sbin/coreos-postinst /postinst
	fi

	use delta_generator && dobin delta_generator

	systemd_dounit systemd/update-engine.service
	systemd_dounit systemd/update-engine-stub.service
	systemd_dounit systemd/update-engine-stub.timer

	systemd_enable_service multi-user.target update-engine.service
	systemd_enable_service multi-user.target update-engine-stub.timer

	insinto /usr/share/dbus-1/services
	doins com.coreos.update1.service

	insinto /usr/share/dbus-1/system.d
	doins com.coreos.update1.conf

	# Install rule to remove old UpdateEngine.conf from /etc
	systemd_dotmpfilesd "${FILESDIR}"/update-engine.conf
}
