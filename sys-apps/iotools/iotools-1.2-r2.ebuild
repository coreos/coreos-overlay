# Copyright 2010 Google Inc.
# Distributed under the terms of the GNU General Public License v2
# $Header: $

inherit eutils toolchain-funcs

DESCRIPTION="Simple commands to access hardware device registers"
HOMEPAGE="http://code.google.com/p/iotools/"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="x86 amd64"
IUSE="hardened"

SRC_URI="http://commondatastorage.googleapis.com/chromeos-localmirror/distfiles/${P}.tar.gz"

src_compile() {
	# If we are on hardened/x86, turn off PIE because the code uses %ebx
	if use x86 ; then
		if use hardened ; then
			epatch "${FILESDIR}"/iotools-1.2.nopie.patch
		fi
	fi

	emake CC="$(tc-getCC)" || die "emake failed"
}

IOTOOLS_COMMANDS="and btr bts busy_loop cmos_read cmos_write cpu_list \
		  cpuid io_read16 io_read32 io_read8 io_write16 io_write32 \
		  io_write8 mmio_dump mmio_read16 mmio_read32 mmio_read64 \
		  mmio_read8 mmio_write16 mmio_write32 mmio_write64 \
		  mmio_write8 not or pci_list pci_read16 pci_read32 pci_read8 \
		  pci_write16 pci_write32 pci_write8 rdmsr rdtsc runon shl \
		  shr smbus_quick smbus_read16 smbus_read8 smbus_readblock \
		  smbus_receive_byte smbus_send_byte smbus_write16 \
		  smbus_write8 smbus_writeblock wrmsr xor"

src_install() {
	dosbin iotools || die

	echo "installing symbolic link shortcuts for iotools"
	# Note: This is done manually because invoking the iotools binary
	# when cross-compiling will likely fail.
	cd "${D}/usr/sbin"
	for COMMAND in ${IOTOOLS_COMMANDS} ; do
		ln -s iotools ${COMMAND}
	done
}
