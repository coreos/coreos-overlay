# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-kernel/vanilla-sources/vanilla-sources-3.7.5.ebuild,v 1.1 2013/01/28 13:18:54 ago Exp $

EAPI=4
CROS_WORKON_COMMIT="19f949f52599ba7c3f67a5897ac6be14bfcb1200"
CROS_WORKON_TREE="19f949f52599ba7c3f67a5897ac6be14bfcb1200"
CROS_WORKON_REPO="https://kernel.googlesource.com/pub/scm/linux/kernel/git/"
CROS_WORKON_PROJECT="torvalds/linux"
inherit cros-workon cros-kernel2

DESCRIPTION="CoreOS kernel"
HOMEPAGE="http://www.kernel.org"
SRC_URI="${KERNEL_URI}"

KEYWORDS="amd64 arm x86"
IUSE="deblob"
