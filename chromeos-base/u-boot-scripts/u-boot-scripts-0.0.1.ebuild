# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2

DESCRIPTION="Tegra2 boot scripts"

LICENSE="BCD"
SLOT="0"
KEYWORDS="arm"
IUSE=""

src_compile() {
	BASE="${FILESDIR}"/boot.scr
	sed 's/\${KERNEL_PART}/2/g;s/\${ROOT_PART}/3/g' "$BASE" >boot-A.scr || die
	sed 's/\${KERNEL_PART}/4/g;s/\${ROOT_PART}/5/g' "$BASE" >boot-B.scr || die

	for script in boot-{A,B}.scr; do
		mkimage -A arm -O linux -T script -C none -a 0 -e 0 \
			-n $script -d $script $script.uimg >/dev/null || die
	done
}

src_install() {
	insinto /boot
	doins boot-{A,B}.scr.uimg || die
}
