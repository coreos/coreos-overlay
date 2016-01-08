# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sec-policy/selinux-virt/selinux-virt-2.20141203-r5.ebuild,v 1.2 2015/06/05 16:10:23 perfinion Exp $
EAPI="5"

IUSE=""
MODS="virt"

inherit selinux-policy-2

DESCRIPTION="SELinux policy for virt"

POLICY_PATCH="${FILESDIR}/virt.diff"

if [[ $PV == 9999* ]] ; then
	KEYWORDS=""
else
	KEYWORDS="amd64 x86"
fi
