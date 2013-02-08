# Copyright (c) 2010 The Chromium OS Authors.  All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header$

EAPI="2"
CROS_WORKON_PROJECT="chromiumos/third_party/tpm-emulator"

inherit cmake-utils cros-workon toolchain-funcs

DESCRIPTION="TPM Emulator with small google-local changes"
LICENSE="GPL-2"
HOMEPAGE="//https://developer.berlios.de/projects/tpm-emulator"
SLOT="0"
IUSE="doc"
KEYWORDS="~amd64 ~arm ~x86"

DEPEND="app-crypt/trousers
	dev-libs/gmp"

RDEPEND="${DEPEND}"

src_configure() {
	tc-export CC CXX LD AR RANLIB NM
	CHROMEOS=1 cmake-utils_src_configure
}

src_compile() {
	cmake-utils_src_compile
	# This would be normally done in kernel module build, but we have
	# copied the kernel module to the kernel tree.
	TPMD_DEV_SRC_DIR="${S}/tpmd_dev/linux"
	TPMD_DEV_BUILD_DIR="${CMAKE_BUILD_DIR}/tpmd_dev/linux"
	mkdir -p "${TPMD_DEV_BUILD_DIR}"
	sed -e "s/\$TPM_GROUP/tss/g" \
		< "${TPMD_DEV_SRC_DIR}/tpmd_dev.rules.in" \
		> "${TPMD_DEV_BUILD_DIR}/tpmd_dev.rules"
}

src_install() {
	# TODO(semenzato): need these for emerge on host, to run tpm_lite tests.
	# insinto /usr/lib
	# doins "${CMAKE_BUILD_DIR}/tpm/libtpm.a"
	# doins "${CMAKE_BUILD_DIR}/crypto/libcrypto.a"
	# doins "${CMAKE_BUILD_DIR}/tpmd/unix/libtpmemu.a"
	# insinto /usr/include
	# doins "${S}/tpmd/unix/tpmemu.h"
	exeinto /usr/sbin
	doexe "${CMAKE_BUILD_DIR}/tpmd/unix/tpmd"
	insinto /etc/udev/rules.d
	TPMD_DEV_BUILD_DIR="${CMAKE_BUILD_DIR}/tpmd_dev/linux"
	RULES_FILE="${TPMD_DEV_BUILD_DIR}/tpmd_dev.rules"
	newins "${RULES_FILE}" 80-tpmd_dev.rules
}
