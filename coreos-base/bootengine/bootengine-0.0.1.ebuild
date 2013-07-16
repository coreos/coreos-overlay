# Copyright (c) 2013 CoreOS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_COMMIT="c3e5455dd4b2ebb5f05f9203ddbe4e24c3af8ede"
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
	sys-kernel/dracut"

src_install() {
	insinto /usr/lib/dracut/modules.d/
	doins -r ${S}/dracut/80gptprio $modules_dir

	chroot /build/amd64-generic dracut --no-kernel --fstab --no-compress /tmp/bootengine.cpio

	insinto /usr/share/bootengine/
	doins /build/amd64-generic/tmp/bootengine.cpio
}
