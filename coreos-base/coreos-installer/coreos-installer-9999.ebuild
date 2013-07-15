# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_PROJECT="coreos/installer"
CROS_WORKON_LOCALNAME="installer"
CROS_WORKON_OUTOFTREE_BUILD=1
CROS_WORKON_REPO="git://github.com"

inherit cros-workon cros-debug cros-au

DESCRIPTION="CoreOS Installer"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE="32bit_au cros_host"

DEPEND="
	chromeos-base/verity
	dev-cpp/gmock"

# TODO(adlr): remove coreutils dep if we move to busybox
RDEPEND="
	app-admin/sudo
	coreos-base/vboot_reference
	dev-util/shflags
	sys-apps/coreutils
	sys-apps/flashrom
	sys-apps/hdparm
	sys-apps/rootdev
	sys-apps/util-linux
	sys-apps/which
	sys-block/parted
	sys-fs/dosfstools
	sys-fs/e2fsprogs"

src_prepare() {
	cros-workon_src_prepare
}

src_configure() {
	# need this to get the verity headers working
	append-cxxflags -I"${SYSROOT}"/usr/include/verity/
	append-cxxflags -I"${SYSROOT}"/usr/include/vboot
	append-ldflags -L"${SYSROOT}"/usr/lib/vboot32

	use 32bit_au && board_setup_32bit_au_env

	cros-workon_src_configure
}

src_compile() {
	# We don't need the installer in the sdk, just helper scripts.
	use cros_host && return 0

	cros-workon_src_compile
}

src_test() {
	# Needed for `cros_run_unit_tests`.
	cros-workon_src_test
}

src_install() {
	cros-workon_src_install
	local path
	if use cros_host ; then
		# Copy chromeos-* scripts to /usr/lib/installer/ on host.
		path="usr/lib/installer"
	else
		path="usr/sbin"
		dobin "${OUT}"/cros_installer
		dosym ${path}/chromeos-postinst /postinst
	fi

	exeinto /${path}
	doexe chromeos-*
}
