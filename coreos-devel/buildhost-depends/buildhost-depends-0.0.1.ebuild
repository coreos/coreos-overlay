# Copyright 2013 The CoreOS Authors
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

DESCRIPTION="Meta ebuild for everything that should be on build hosts."
HOMEPAGE="http://coreos.com/docs/sdk/"
SRC_URI=""

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

DEPEND="
	coreos-devel/coreos-buildbot
	coreos-devel/sdk-depends
	dev-util/buildbot-slave
	dev-util/catalyst[ccache]
	"
RDEPEND="${DEPEND}"
