# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2

inherit toolchain-funcs

DESCRIPTION="Empty (null) ebuild which satisifies chromeos-hwid.
This is overridden with board specific approved HWIDs in chromeos-overlay."

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

RDEPEND=""
DEPEND=""

#
# WARNING: Nothing should be added to this ebuild.  This ebuild is overriden
# in chromeos overlay.
#
