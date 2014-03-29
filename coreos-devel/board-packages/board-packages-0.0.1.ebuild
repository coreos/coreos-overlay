# Copyright 2013 The CoreOS Authors
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

DESCRIPTION="Meta ebuild for building all binary packages."
HOMEPAGE="http://coreos.com/docs/sdk/"
SRC_URI=""

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

DEPEND=""
RDEPEND="
	coreos-base/coreos
	coreos-base/coreos-dev
	"
	# dev-lang/python-oem
