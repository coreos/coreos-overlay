# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4

DESCRIPTION="Host dependencies BSP virtual package. Override this virtual
package in a private host overlay by adding a virtual/hard-host-depends-bsp
version 2 ebuild, and have that package RDEPEND on any host dependencies needed
by the private overlay."
HOMEPAGE="http://src.chromium.org"

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 x86"
