# Copyright (c) 2012 The CoreOS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2

DESCRIPTION="CoreOS Kernel virtual package"
HOMEPAGE="http://www.coreos.com"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="-kernel_next -kernel_sources"

RDEPEND="
	kernel_next? ( sys-kernel/coreos-kernel-next[kernel_sources=] )
	!kernel_next? ( sys-kernel/coreos-kernel[kernel_sources=] )
"
