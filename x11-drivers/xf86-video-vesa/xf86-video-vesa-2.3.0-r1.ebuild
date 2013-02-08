# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/x11-drivers/xf86-video-vesa/xf86-video-vesa-2.3.0.ebuild,v 1.3 2010/02/08 15:31:28 fauli Exp $

inherit x-modular

DESCRIPTION="Generic VESA video driver"
KEYWORDS="-* ~alpha amd64 ~ia64 x86 ~x86-fbsd"
RDEPEND=">=x11-base/xorg-server-1.0.99"
DEPEND="${RDEPEND}
	x11-proto/fontsproto
	x11-proto/randrproto
	x11-proto/renderproto
	x11-proto/xextproto
	x11-proto/xproto"
PATCHES=(
	"${FILESDIR}/no-dga.patch"
	"${FILESDIR}/2.3.0-r1-domainiobase.patch"
	"${FILESDIR}/2.3.0-r1-xf86MapDomainMemory.patch"
)

