# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_COMMIT="589e763e493ff2544d2f98cdfaceed6865e066b8"
CROS_WORKON_TREE="aac8fef67a3b7b56c9b6cb32af538d32e9e70b15"
CROS_WORKON_PROJECT="chromiumos/third_party/seabios"

inherit toolchain-funcs

DESCRIPTION="Open Source implementation of X86 BIOS"
HOMEPAGE="http://www.coreboot.org/SeaBIOS"
LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE=""

RDEPEND=""
DEPEND="
	       virtual/chromeos-coreboot
	       sys-apps/coreboot-utils
"
CROS_WORKON_LOCALNAME="seabios"

# This must be inherited *after* EGIT/CROS_WORKON variables defined
inherit cros-workon

# Directory where the generated files are looked for and placed.
CROS_FIRMWARE_IMAGE_DIR="/firmware"
CROS_FIRMWARE_ROOT="${ROOT%/}${CROS_FIRMWARE_IMAGE_DIR}"

create_seabios_cbfs() {
	local oprom=${CROS_FIRMWARE_ROOT}/pci????,????.rom
	local seabios_cbfs=seabios.cbfs
	local cbfs_size=$(( 2*1024*1024 ))
	local bootblock=$( mktemp )

	# Create a dummy bootblock to make cbfstool happy
	dd if=/dev/zero of=$bootblock count=1 bs=64
	# Create empty CBFS
	cbfstool ${seabios_cbfs} create -s ${cbfs_size} -B $bootblock
	# Clean up
	rm $bootblock
	# Add SeaBIOS binary to CBFS
	cbfstool ${seabios_cbfs} add-payload -f out/bios.bin.elf -n payload -c lzma
	# Add VGA option rom to CBFS
	cbfstool ${seabios_cbfs} add -f $oprom -n $( basename $oprom ) -t optionrom
	# Add additional configuration
	cbfstool ${seabios_cbfs} add -f bootorder -n bootorder -t raw
	cbfstool ${seabios_cbfs} add -f boot-menu-wait -n boot-menu-wait -t raw
	# Print CBFS inventory
	cbfstool ${seabios_cbfs} print
	# Fix up CBFS to live at 0xffc00000. The last four bytes of a CBFS
	# image are a pointer to the CBFS master header. Per default a CBFS
	# lives at 4G - rom size, and the CBFS master header ends up at
	# 0xffffffa0. However our CBFS lives at 4G-4M and is 2M in size, so
	# the CBFS master header is at 0xffdfffa0 instead. The two lines
	# below correct the according byte in that pointer to make all CBFS
	# parsing code happy. In the long run we should fix cbfstool and
	# remove this workaround.
	/bin/echo -ne \\0737 | dd of=${seabios_cbfs} \
			seek=$(( ${cbfs_size} - 2 )) bs=1 conv=notrunc
}

src_compile() {
	export LD="$(tc-getLD).bfd"
	export CC="$(tc-getCC) -fuse-ld=bfd"
	emake defconfig || die "${P}: configuration failed"
	emake || die "${P}: compilation failed"
	create_seabios_cbfs
}

src_install() {
	dodir /firmware
	insinto /firmware
	doins out/bios.bin.elf || die
	doins seabios.cbfs || die
}
