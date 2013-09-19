# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/systemd-sysv-utils/systemd-sysv-utils-194.ebuild,v 1.1 2012/10/04 20:03:07 mgorny Exp $

EAPI=5

MY_P=systemd-${PV}

DESCRIPTION="sysvinit compatibility symlinks and manpages"
HOMEPAGE="http://www.freedesktop.org/wiki/Software/systemd"
SRC_URI="http://www.freedesktop.org/software/systemd/${MY_P}.tar.xz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE=""

RDEPEND="!sys-apps/sysvinit
	>=sys-apps/systemd-${PV}"

S=${WORKDIR}/${MY_P}/man

src_install() {
	for app in halt poweroff reboot runlevel shutdown telinit; do
		doman ${app}.8
		dosym ../usr/bin/systemctl /sbin/${app}
	done

	newman init.1 init.8
	dosym ../usr/lib/systemd/systemd /sbin/init
}
