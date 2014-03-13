# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

inherit multiprocessing toolchain-funcs

DESCRIPTION="EDK II Open Source UEFI Firmware"
HOMEPAGE="http://tianocore.sourceforge.net"

LICENSE="BSD-2"
SLOT="0"
IUSE="+qemu"

if [[ ${PV} == 99999 ]]; then
	inherit subversion
	ESVN_REPO_URI="https://svn.code.sf.net/p/edk2/code/trunk/edk2"
	KEYWORDS="-* ~amd64"
else
	MY_P="edk2-${PV}"
	S="${WORKDIR}/${MY_P}"
	SRC_URI="http://storage.core-os.net/mirror/snapshots/${MY_P}.tar.xz"
	KEYWORDS="-* amd64"
fi

DEPEND="sys-power/iasl"
RDEPEND="qemu? ( app-emulation/qemu )"

edk_build() {
	local package="$1"

	# Use a subshell since edksetup.sh expects to be sourced.
	( export WORKSPACE="${S}" EDK_TOOLS_PATH="${S}/BaseTools"
	source edksetup.sh BaseTools || exit $?
	build -p "${package}" \
		-a "${TARGET_ARCH}" \
		-b "${TARGET_NAME}" \
		-t "${TARGET_TOOLS}" \
		-n "${BUILD_JOBS}" || exit $?
	) || die "Building EDK package ${package} failed"
}

src_configure() {
	./edksetup.sh || die

	BUILD_JOBS=$(makeopts_jobs)
	TARGET_NAME="RELEASE"
	TARGET_TOOLS="GCC$(gcc-version | tr -d .)"
	case $ARCH in
		amd64)	TARGET_ARCH=X64 ;;
		#x86)	TARGET_ARCH=IA32 ;;
		*)		die "Unsupported $ARCH" ;;
	esac
}

src_compile() {
	emake ARCH=${TARGET_ARCH} -C BaseTools -j1

	edk_build OvmfPkg/OvmfPkgX64.dsc
}

src_install() {
	insinto /usr/share/${PN}
	doins Build/OvmfX64/${TARGET_NAME}_${TARGET_TOOLS}/FV/OVMF.fd
	dosym OVMF.fd /usr/share/${PN}/bios.bin

	if use qemu; then
		dosym ../${PN}/OVMF.fd /usr/share/qemu/efi-bios.bin
	fi
}
