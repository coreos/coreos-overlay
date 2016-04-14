# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

inherit eutils multiprocessing toolchain-funcs

DESCRIPTION="EDK II Open Source UEFI Firmware"
HOMEPAGE="http://tianocore.sourceforge.net"

LICENSE="BSD-2"
SLOT="0"
IUSE="debug +secure-boot"

if [[ ${PV} == 99999 ]]; then
	inherit subversion
	ESVN_REPO_URI="https://svn.code.sf.net/p/edk2/code/trunk/edk2"
	KEYWORDS="-* ~arm64"
else
	MY_P="edk2-${PV}"
	S="${WORKDIR}/${MY_P}"
	SRC_URI="http://storage.core-os.net/mirror/snapshots/${MY_P}.tar.xz"
	KEYWORDS="-* arm64"
fi

OPENSSL_PV="1.0.2d"
OPENSSL_P="openssl-${OPENSSL_PV}"
SRC_URI+=" mirror://openssl/source/${OPENSSL_P}.tar.gz"

src_prepare() {
	# aarch64 gcc gets confused by -pie
	epatch "${FILESDIR}/edk2-nopie.patch"

	if use secure-boot; then
		local openssllib="${S}/CryptoPkg/Library/OpensslLib"
		mv "${WORKDIR}/${OPENSSL_P}" "${openssllib}" || die
		cd "${openssllib}/${OPENSSL_P}"
		epatch "${openssllib}/EDKII_${OPENSSL_P}.patch"
		cd "${openssllib}"
		sh -e ./Install.sh || die
		cd "${S}"
	fi
}

src_configure() {
	./edksetup.sh || die

	TARGET_NAME=$(usex debug DEBUG RELEASE)
	TARGET_TOOLS="GCC$(gcc-version | tr -d .)"
	case $ARCH in
		arm64)  TARGET_ARCH=AARCH64 ;;
		*)		die "Unsupported $ARCH" ;;
	esac
}

src_compile() {
	emake ARCH=${TARGET_ARCH} -C BaseTools -j1

	export GCC49_AARCH64_PREFIX="${CHOST}-"
	. ./edksetup.sh || die
	build \
		-a ${TARGET_ARCH} \
		-b ${TARGET_NAME} \
		-p ArmVirtPkg/ArmVirtQemu.dsc \
		-t ${TARGET_TOOLS} \
		-n $(makeopts_jobs) \
		-D SECURE_BOOT_ENABLE=$(usex secure-boot TRUE FALSE) \
		-D FD_SIZE_2MB || die "Building ArmVirtPkg failed"
}

src_install() {
	local fv="Build/ArmVirtQemu-${TARGET_ARCH}/${TARGET_NAME}_${TARGET_TOOLS}/FV"
	insinto /usr/share/${PN}
	doins "${fv}"/QEMU_EFI.fd
	dosym QEMU_EFI.fd /usr/share/${PN}/bios.bin
}

