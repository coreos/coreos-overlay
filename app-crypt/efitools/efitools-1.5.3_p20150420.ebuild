# Copyright 1999-2013 Gentoo Foundation
# Copyright 2015 CoreOS, Inc
# Distributed under the terms of the GNU General Public License v2

EAPI="4"

DESCRIPTION="Tools for manipulating UEFI secure boot platforms"
HOMEPAGE="git://git.kernel.org/pub/scm/linux/kernel/git/jejb/efitools.git"
SRC_URI="http://storage.core-os.net/mirror/snapshots/efitools-${PV}.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE=""

RDEPEND="dev-libs/openssl
	sys-apps/util-linux"
DEPEND="${RDEPEND}
	sys-apps/help2man
	sys-boot/gnu-efi
	app-crypt/sbsigntool
	virtual/pkgconfig
	dev-perl/File-Slurp"
