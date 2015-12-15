# Copyright 2013 The CoreOS Authors
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

DESCRIPTION="Meta ebuild for everything that should be in the SDK."
HOMEPAGE="http://coreos.com/docs/sdk/"
SRC_URI=""

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

DEPEND="
	app-admin/sudo
	app-admin/updateservicectl
	app-arch/pbzip2
	app-emulation/open-vmdk
	app-misc/jq
	app-portage/eix
	app-shells/bash-completion
	coreos-base/hard-host-depends
	coreos-base/coreos-sb-keys
	coreos-devel/mantle
	dev-perl/Capture-Tiny
	dev-python/setuptools
	dev-util/boost-build
	dev-util/checkbashisms
	dev-vcs/repo
	net-misc/curl
	sys-apps/debianutils
	sys-apps/iproute2
	sys-apps/seismograph
	sys-boot/grub
	sys-boot/shim
	sys-devel/crossdev
	sys-firmware/edk2-ovmf
	sys-fs/btrfs-progs
	sys-fs/cryptsetup
	"

# Must match the build-time dependencies listed in selinux-policy-2.eclass
DEPEND="${DEPEND}
	>=sys-apps/checkpolicy-2.0.21
	>=sys-apps/policycoreutils-2.0.82
	sys-devel/m4"

# Required by dev-lang/spidermonkey-1.8.5
DEPEND="${DEPEND}
	sys-devel/autoconf:2.1"

RDEPEND="${DEPEND}"
