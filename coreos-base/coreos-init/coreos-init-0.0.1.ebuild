# Copyright (c) 2013 The CoreOS Authors. All rights reserved.
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_COMMIT="1a02594160810ace0059ad3c9a7bcfae00ebe934"
CROS_WORKON_PROJECT="coreos/init"
CROS_WORKON_LOCALNAME="init"

inherit cros-workon systemd

DESCRIPTION="Init scripts for CoreOS"
HOMEPAGE="http://www.coreos.com/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="nfs"

DEPEND=""
RDEPEND="sys-apps/systemd"

src_install() {
	into /	# We want /sbin, not /usr/sbin, etc.

	dosbin coreos_startup

	systemd_dounit coreos-startup.service
	systemd_enable_service basic.target coreos-startup.service

	systemd_dounit update-engine.service
	systemd_enable_service multi-user.target update-engine.service

	systemd_enable_service multi-user.target sshd.socket
}
