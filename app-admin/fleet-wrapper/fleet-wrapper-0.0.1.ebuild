#
# Copyright (c) 2016 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=6

inherit systemd

DESCRIPTION="fleet (Distributed Init System)"
HOMEPAGE="https://github.com/coreos/fleet"
KEYWORDS="amd64"

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

RDEPEND="
	!app-admin/fleet
	>=app-emulation/rkt-1.9.1[rkt_stage1_fly]
"

# work around ${WORKDIR}/${P} not existing
S=${WORKDIR}

src_install() {
	exeinto /usr/lib/coreos
	doexe "${FILESDIR}"/fleet-wrapper

	systemd_dounit "${FILESDIR}"/fleet.service
	systemd_dounit "${FILESDIR}"/fleet.socket
	systemd_dotmpfilesd "${FILESDIR}"/tmpfiles.d/fleet.conf

	# Grant systemd1 access for fleet user
	insinto /usr/share/polkit-1/rules.d
	doins "${FILESDIR}"/98-fleet-org.freedesktop.systemd1.rules

	# Install sysusers.d snippet which adds fleet group and adds core user into it
	insinto /usr/lib/sysusers.d/
	newins "${FILESDIR}"/sysusers.d/fleet.conf fleet.conf
}
