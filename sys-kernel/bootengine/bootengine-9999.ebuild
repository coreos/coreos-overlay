# Copyright (c) 2013 CoreOS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/bootengine"
CROS_WORKON_LOCALNAME="bootengine"
CROS_WORKON_OUTOFTREE_BUILD=1
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm ~arm64 ~x86"
else
	CROS_WORKON_COMMIT="deba0732daec569545cf456f0cc514f17c7529b5"
	KEYWORDS="amd64 arm arm64 x86"
fi

inherit cros-workon cros-debug

DESCRIPTION="CoreOS Bootengine"
SRC_URI=""

LICENSE="BSD"
SLOT="0/${PVR}"

src_install() {
	insinto /usr/lib/dracut/modules.d/
	doins -r dracut/.
	dosbin update-bootengine

	# must be executable since dracut's install scripts just
	# re-use existing filesystem permissions during initrd creation.
	chmod +x "${D}"/usr/lib/dracut/modules.d/10*-generator/*-generator \
		"${D}"/usr/lib/dracut/modules.d/10diskless-generator/diskless-btrfs \
		"${D}"/usr/lib/dracut/modules.d/30ignition/ignition-generator \
		"${D}"/usr/lib/dracut/modules.d/30ignition/ignition-setup.sh \
		"${D}"/usr/lib/dracut/modules.d/30ignition/retry-umount.sh \
		"${D}"/usr/lib/dracut/modules.d/99setup-root/initrd-setup-root \
		|| die chmod
}
