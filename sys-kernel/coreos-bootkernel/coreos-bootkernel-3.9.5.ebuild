# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-kernel/vanilla-sources/vanilla-sources-3.7.5.ebuild,v 1.1 2013/01/28 13:18:54 ago Exp $

EAPI=4
CROS_WORKON_COMMIT="da3093ad9f44ba53c3f254b4392f3c742dee41d3"
CROS_WORKON_TREE="0ff44b050e7f57e7d3743a44af2c9211da30bdb4"
CROS_WORKON_REPO="git://github.com"
CROS_WORKON_PROJECT="coreos/linux"
inherit cros-workon cros-bootkernel

DEPEND="
	sys-devel/bc
"

DESCRIPTION="CoreOS kernel"
HOMEPAGE="http://www.kernel.org"
SRC_URI="${KERNEL_URI}"

KEYWORDS="amd64 arm x86"
IUSE="deblob"
