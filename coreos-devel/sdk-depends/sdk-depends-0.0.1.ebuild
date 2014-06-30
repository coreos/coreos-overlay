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
	app-admin/updatectl
	app-arch/pbzip2
	app-shells/bash-completion
	coreos-base/hard-host-depends
	dev-python/setuptools
	dev-util/boost-build
	dev-util/checkbashisms
	dev-vcs/repo
	net-misc/curl
	sys-apps/debianutils
	sys-apps/iproute2
	sys-devel/crossdev
	sys-fs/btrfs-progs
	"
RDEPEND="${DEPEND}"
