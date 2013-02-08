# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/x11-apps/intel-gpu-tools/intel-gpu-tools-1.0.2.ebuild,v 1.3 2009/12/10 18:16:44 fauli Exp $

# Must be before x-modular eclass is inherited
#SNAPSHOT="yes"

inherit x-modular

DESCRIPTION="intel gpu userland tools"
KEYWORDS="amd64 x86"
IUSE=""
RESTRICT="test"

DEPEND="
	>=x11-libs/libdrm-2.4.6
	>=x11-libs/libpciaccess-0.10
	x11-libs/cairo"
RDEPEND="${DEPEND}"
