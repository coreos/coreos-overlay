# Copyright (c) 2013 CoreOS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_PROJECT="coreos/bootengine"
CROS_WORKON_LOCALNAME="bootengine"
CROS_WORKON_OUTOFTREE_BUILD=1
CROS_WORKON_REPO="git://github.com"

inherit cros-workon cros-debug cros-au

DESCRIPTION="CoreOS Bootengine"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE="32bit_au cros_host"

DEPEND="
	sys-kernel/dracut"

src_install() {
	modules_dir=${D}/usr/lib/dracut/modules.d/
	mkdir -p $modules_dir
	cp -R dracut/80gptprio $modules_dir

	mkdir ${D}/boot
	for i in /boot/vmlinuz-*boot_kernel*; do
		ver=${i##*vmlinuz-}
		dracut --kver ${ver} ${D}/boot/initramfs-${ver}.img
	done
}
