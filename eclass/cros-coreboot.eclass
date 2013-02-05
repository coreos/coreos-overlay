# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

# @ECLASS: cros-coreboot.eclass
# @MAINTAINER:
# The Chromium OS Authors
# @BLURB: Unifies logic for building coreboot images for Chromium OS.

[[ ${EAPI} != "4" ]] && die "Only EAPI=4 is supported"

inherit toolchain-funcs

DESCRIPTION="coreboot x86 firmware"
HOMEPAGE="http://www.coreboot.org"
LICENSE="GPL-2"
SLOT="0"
IUSE="em100-mode"


RDEPEND="!sys-boot/chromeos-coreboot"

DEPEND="sys-power/iasl
	sys-apps/coreboot-utils
	sys-boot/chromeos-mrc
	"

# @ECLASS-VARIABLE: COREBOOT_BOARD
# @DESCRIPTION:
# Coreboot Configuration name.
: ${COREBOOT_BOARD:=}

# @ECLASS-VARIABLE: COREBOOT_BUILD_ROOT
# @DESCRIPTION:
# Build directory root
: ${COREBOOT_BUILD_ROOT:=}

[[ -z ${COREBOOT_BOARD} ]] && die "COREBOOT_BOARD must be set"
[[ -z ${COREBOOT_BUILD_ROOT} ]] && die "COREBOOT_BUILD_ROOT must be set"

cros-coreboot_pre_src_prepare() {
	cp configs/config.${COREBOOT_BOARD} .config
}

cros-coreboot_src_compile() {
	tc-export CC
	local board="${COREBOOT_BOARD}"
	local build_root="${COREBOOT_BUILD_ROOT}"

	# Set KERNELREVISION (really coreboot revision) to the ebuild revision
	# number followed by a dot and the first seven characters of the git
	# hash. The name is confusing but consistent with the coreboot
	# Makefile.
	local sha1v="${VCSID/*-/}"
	export KERNELREVISION=".${PV}.${sha1v:0:7}"

	# Firmware related binaries are compiled with a 32-bit toolchain
	# on 64-bit platforms
	if use amd64 ; then
		export CROSS_COMPILE="i686-pc-linux-gnu-"
		export CC="${CROSS_COMPILE}-gcc"
	else
		export CROSS_COMPILE=${CHOST}-
	fi

	elog "Toolchain:\n$(sh util/xcompile/xcompile)\n"
	emake obj="${build_root}" oldconfig
	emake obj="${build_root}"

	# Modify firmware descriptor if building for the EM100 emulator.
	if use em100-mode; then
		ifdtool --em100 "${build_root}/coreboot.rom" || die
		mv "${build_root}/coreboot.rom"{.new,} || die
	fi

	# Build cbmem for the target
	cd util/cbmem
	emake clean
	CROSS_COMPILE="${CHOST}-" emake
}

cros-coreboot_src_install() {
	dobin util/cbmem/cbmem
	insinto /firmware
	newins "${COREBOOT_BUILD_ROOT}/coreboot.rom" coreboot.rom
	OPROM=$( awk 'BEGIN{FS="\""} /CONFIG_VGA_BIOS_FILE=/ { print $2 }' \
		configs/config.${COREBOOT_BOARD} )
	CBFSOPROM=pci$( awk 'BEGIN{FS="\""} /CONFIG_VGA_BIOS_ID=/ { print $2 }' \
		configs/config.${COREBOOT_BOARD} ).rom
	newins ${OPROM} ${CBFSOPROM}
}

EXPORT_FUNCTIONS src_compile src_install pre_src_prepare
