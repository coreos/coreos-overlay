# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_PROJECT="chromiumos/platform/system_api"

inherit cros-workon toolchain-funcs

DESCRIPTION="Chrome OS system API (D-Bus service names, etc.)"
HOMEPAGE="http://www.chromium.org/"
LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"

# Likewise, block libchromeos-0.0.1-r78 or older, that installs
# dbus/service_constants.h. TODO(satorux): Remove this after a month.
RDEPEND="!<=chromeos-base/libchromeos-0.0.1-r78"

DEPEND="${RDEPEND}"

CROS_WORKON_LOCALNAME="$(basename ${CROS_WORKON_PROJECT})"

src_install() {
	insinto /usr/include/chromeos/dbus
	doins -r dbus/*
}
