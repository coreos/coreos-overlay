# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/kexec-tools/kexec-tools-2.0.4-r1.ebuild,v 1.1 2013/03/30 13:01:49 jlec Exp $

EAPI=2

inherit eutils flag-o-matic

DESCRIPTION="Load another kernel from the currently executing Linux kernel"
HOMEPAGE="http://kernel.org/pub/linux/utils/kernel/kexec/"
SRC_URI="http://beef.coreos.com/kexec"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~x86"

src_install() {
	wget -O ${T}/kexec ${SRC_URI}
	dosbin ${T}/kexec
}
