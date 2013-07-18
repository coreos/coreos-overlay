# Copyright (c) 2013 CoreOS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_COMMIT="23bf5d73f71886743cee0c05f6f9f707877b9883"
CROS_WORKON_PROJECT="coreos/bootengine"
CROS_WORKON_LOCALNAME="bootengine"
CROS_WORKON_OUTOFTREE_BUILD=1
CROS_WORKON_REPO="git://github.com"

inherit cros-workon cros-debug cros-au

DESCRIPTION="CoreOS Bootengine"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 x86"

DEPEND="
	sys-apps/kexec-tools
	sys-kernel/dracut"

src_install() {
	insinto /usr/lib/dracut/modules.d/
	doins -r ${S}/dracut/80gptprio $modules_dir
}

# We are bad, we want to get around the sandbox.  So do the creation of the
# cpio image in pkg_postinst() where we are free to mount filesystems, chroot,
# and other fun stuff.
pkg_postinst() {
	mount -t proc proc ${ROOT}/proc
	mount --rbind /dev ${ROOT}/dev
	mount --rbind /sys ${ROOT}/sys
	mount --rbind /run ${ROOT}/run

	# --host-only "should" mean that we only include the stuff that this build
	# root needs.  The keyboard tables are all still being included, which we
	# need to figure out how to remove someday.
	chroot ${ROOT} dracut --host-only --force --no-kernel --fstab --no-compress /tmp/bootengine.cpio

	umount ${ROOT}/proc
	umount ${ROOT}/dev/pts	# trust me, it's there, unmount it.
	umount ${ROOT}/dev
	umount ${ROOT}/sys
	umount ${ROOT}/run

	# as we are not in src_install() insinto and doins do not work here, so
	# manually copy the file around
	cpio=${ROOT}/tmp/bootengine.cpio
	chmod 644 ${cpio}
	mkdir ${ROOT}/usr/share/bootengine/
	cp ${cpio} ${ROOT}/usr/share/bootengine/
	rm ${cpio}
}
