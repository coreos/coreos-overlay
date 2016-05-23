# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_PROJECT="chromiumos/platform/crostestutils"

inherit cros-workon

DESCRIPTION="Host test utilities for ChromiumOS"
HOMEPAGE="http://www.chromium.org/"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"

CROS_WORKON_LOCALNAME="crostestutils"
CROS_WORKON_LOCALDIR="src/platform"


RDEPEND="app-emulation/qemu
	app-portage/gentoolkit
	app-shells/bash
	coreos-base/cros-devutils
	dev-util/crosutils
	"

# These are all either bash / python scripts.  No actual builds DEPS.
DEPEND=""

# Use default src_compile and src_install which use Makefile.
