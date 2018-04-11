# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

inherit eutils multiprocessing toolchain-funcs

DESCRIPTION="EDK II Open Source UEFI Firmware"
HOMEPAGE="https://github.com/tianocore/tianocore.github.io/wiki/EDK-II"

LICENSE="BSD-2"
SLOT="0"
IUSE="debug +qemu +secure-boot"

if [[ ${PV} == 99999 ]]; then
	inherit git-r3
	EGIT_REPO_URI="https://github.com/tianocore/edk2.git"
	KEYWORDS="-* ~arm64 ~amd64"
else
	EDK2_PV="vUDK2017"
	S="${WORKDIR}/edk2-${EDK2_PV}"
	SRC_URI="https://github.com/tianocore/edk2/archive/${EDK2_PV}.tar.gz"
	KEYWORDS="-* arm64 amd64"
fi

OPENSSL_PV="1.1.0f"
OPENSSL_P="openssl-${OPENSSL_PV}"
SRC_URI+=" https://www.openssl.org/source/${OPENSSL_P}.tar.gz"

DEPEND="
	amd64? (
		>=dev-lang/nasm-2.10.0
		sys-power/iasl
	)"

RDEPEND="
	!sys-firmware/edk2-armvirt
	!sys-firmware/edk2-ovmf
	"

src_unpack() {
	[[ ${EGIT_REPO_URI} ]] && git-r3_src_unpack
	unpack ${A}
}

src_prepare() {
	epatch "${FILESDIR}/${PN}-2017.06-edksetup.patch"
	epatch "${FILESDIR}/${PN}-2017.06-BaseTools.patch"

	if use secure-boot; then
		local openssllib="${S}/CryptoPkg/Library/OpensslLib"
		mv "${WORKDIR}/${OPENSSL_P}" "${openssllib}/openssl" || \
			die "openssl setup failed."
	fi
}

src_configure() {
	./edksetup.sh || die "edksetup.sh failed."

	TARGET_NAME=$(usex debug DEBUG RELEASE)
	TARGET_TOOLS="GCC$(gcc-version | tr -d .)"
	[[ $TARGET_TOOLS == GCC[5-9]* ]] && TARGET_TOOLS=GCC5

	case ${ARCH} in
	amd64)	TARGET_ARCH=X64 ;;
	arm64)	TARGET_ARCH=AARCH64 ;;
	*)	die "Unsupported ${ARCH}" ;;
	esac
}

src_compile() {
	# The BaseTools makefile has a conflicting ARCH variable.
	local arch_save=${ARCH}
	unset ARCH
	emake -C BaseTools -j1
	ARCH=${arch_save}

	export "${TARGET_TOOLS}_AARCH64_PREFIX=${CHOST}-"
	source ./edksetup.sh || die "edksetup.sh failed."

	case ${ARCH} in
	amd64)
		./OvmfPkg/build.sh \
			-a "${TARGET_ARCH}" \
			-b "${TARGET_NAME}" \
			-t "${TARGET_TOOLS}" \
			-n $(makeopts_jobs) \
			-D SECURE_BOOT_ENABLE=$(usex secure-boot TRUE FALSE) \
			-D FD_SIZE_2MB \
			|| die "edk2 build failed."
		;;
	arm64)
		build \
			-a ${TARGET_ARCH} \
			-b ${TARGET_NAME} \
			-p ArmVirtPkg/ArmVirtQemu.dsc \
			-t ${TARGET_TOOLS} \
			-n $(makeopts_jobs) \
			-D SECURE_BOOT_ENABLE=$(usex secure-boot TRUE FALSE) \
			-D FD_SIZE_2MB \
			|| die "edk2 build failed."
		;;
	*)
		die "Unsupported ${ARCH}"
		;;
	esac
}

src_install() {
	insinto /usr/share/${PN}

	case ${ARCH} in
	amd64)
		local fv="Build/OvmfX64/${TARGET_NAME}_${TARGET_TOOLS}/FV"
		doins "${fv}"/OVMF{,_CODE,_VARS}.fd
		dosym OVMF.fd /usr/share/${PN}/bios.bin

		if use qemu; then
			dosym ../${PN}/OVMF.fd /usr/share/qemu/efi-bios.bin
		fi
		;;
	arm64)
		local fv="Build/ArmVirtQemu-${TARGET_ARCH}/${TARGET_NAME}_${TARGET_TOOLS}/FV"
		doins "${fv}"/QEMU_EFI.fd
		dosym QEMU_EFI.fd /usr/share/${PN}/bios.bin
		;;
	esac
}

