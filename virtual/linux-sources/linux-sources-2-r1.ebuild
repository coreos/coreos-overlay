# Copyright (c) 2012 The CoreOS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2

DESCRIPTION="CoreOS Kernel virtual package"
HOMEPAGE="http://www.coreos.com"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm arm64 x86"
IUSE="cros_host"

RDEPEND="
	!cros_host? ( sys-kernel/coreos-kernel )
"
