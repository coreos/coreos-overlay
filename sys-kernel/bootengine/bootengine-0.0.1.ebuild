# Copyright (c) 2013 CoreOS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_COMMIT="2c80368fb150a18fee29ff2145458956fc0751cc"
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
	app-arch/gzip
	app-shells/bash
	coreos-base/vboot_reference
	sys-apps/coreutils
	sys-apps/findutils
	sys-apps/grep
	sys-apps/kbd
	sys-apps/kexec-tools
	sys-apps/less
	sys-apps/sed
	sys-apps/systemd
	sys-apps/systemd-sysv-utils
	sys-apps/util-linux
	sys-kernel/dracut
	virtual/udev
	"
RDEPEND="${DEPEND}"

src_install() {
	insinto /usr/lib/dracut/modules.d/
	doins -r dracut/.
	dosbin update-bootengine
}

# We are bad, we want to get around the sandbox.  So do the creation of the
# cpio image in pkg_postinst() where we are free to mount filesystems, chroot,
# and other fun stuff.
pkg_postinst() {
	if [[ -n "${ROOT}" ]]; then
		${ROOT}/usr/sbin/update-bootengine -m -c ${ROOT} || die
	else
		update-bootengine || die
	fi
}
