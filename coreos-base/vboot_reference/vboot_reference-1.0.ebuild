# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_COMMIT="28f202d45b4c33efed64e4af906a95b26e7785b4"
CROS_WORKON_PROJECT="coreos/vboot_reference"
CROS_WORKON_REPO="git://github.com"

inherit cros-debug cros-workon cros-au

DESCRIPTION="Chrome OS verified boot tools"

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="cros_host"

RDEPEND="dev-libs/libyaml
	dev-libs/openssl
	sys-apps/util-linux"
DEPEND="app-crypt/trousers
	${RDEPEND}"

src_compile() {
	mkdir "${S}"/build-main
	tc-export CC AR CXX PKG_CONFIG
	cros-debug-add-NDEBUG
	# Vboot reference knows the flags to use
	unset CFLAGS
	emake BUILD="${S}"/build-main \
	      ARCH=$(tc-arch) all
	unset CC AR CXX PKG_CONFIG
}

src_test() {
	emake BUILD="${S}"/build-main \
	      ARCH=$(tc-arch) runtests
}

src_install() {
	einfo "Installing programs"
	# Installing on the host
	emake BUILD="${S}"/build-main DESTDIR="${D}/usr/bin" install

	# Install public headers to /build/${BOARD}/usr/include/vboot
	einfo "Installing header files"
	insinto /usr/include/vboot
	doins firmware/include/* host/include/*

	einfo "Installing host library"
	dolib.a build-main/libvboot_host.a
}
