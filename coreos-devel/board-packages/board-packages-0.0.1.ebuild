# Copyright 2013 The CoreOS Authors
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

DESCRIPTION="Meta ebuild for building all binary packages."
HOMEPAGE="http://coreos.com/docs/sdk/"
SRC_URI=""

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm64"
IUSE=""

DEPEND=""
RDEPEND="
	amd64? (
		app-emulation/google-compute-engine
		app-emulation/open-vm-tools
		app-emulation/wa-linux-agent
		coreos-base/nova-agent-container
		coreos-base/nova-agent-watcher
		dev-lang/python-oem
	)
	arm64? (
		sys-boot/grub
		sys-firmware/edk2-armvirt
	)
	coreos-base/coreos
	coreos-base/coreos-dev
	"
