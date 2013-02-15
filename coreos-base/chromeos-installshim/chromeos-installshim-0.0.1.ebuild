# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4

DESCRIPTION="Chrome OS Install Shim (meta package)"
HOMEPAGE="http://src.chromium.org"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm x86"

X86_DEPEND="
	sys-boot/syslinux
"

# Factory installer
RDEPEND="
	x86? ( ${X86_DEPEND} )
	amd64? ( ${X86_DEPEND} )
	arm? (
		chromeos-base/u-boot-scripts
	)
	app-arch/sharutils
	app-crypt/trousers
	app-shells/bash
	app-shells/dash
	chromeos-base/board-devices
	chromeos-base/chromeos-auth-config
	chromeos-base/chromeos-base
	chromeos-base/chromeos-factoryinstall
	chromeos-base/chromeos-init
	chromeos-base/dev-install
	chromeos-base/vboot_reference
	net-firewall/iptables
	net-misc/tlsdate
	>=sys-apps/baselayout-2.0.0
	sys-apps/coreutils
	sys-apps/dbus
	sys-apps/flashrom
	sys-apps/grep
	sys-apps/mawk
	sys-apps/module-init-tools
	sys-apps/mosys
	sys-apps/net-tools
	sys-apps/pv
	sys-apps/rootdev
	sys-apps/sed
	sys-apps/shadow
	sys-apps/upstart
	sys-apps/util-linux
	sys-apps/which
	sys-auth/pam_pwdfile
	sys-fs/e2fsprogs
	sys-fs/udev
	sys-process/lsof
	sys-process/procps
	virtual/chromeos-bsp
"

S=${WORKDIR}
