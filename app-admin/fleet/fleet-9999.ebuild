# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/fleet"
CROS_WORKON_LOCALNAME="fleet"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/fleet"
inherit coreos-doc cros-workon coreos-go systemd

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="c05214e9019b58c0beebb6bcc907ea1ee61def4b"  # tag v0.11.5
	KEYWORDS="amd64 arm64"
fi

DESCRIPTION="fleet"
HOMEPAGE="https://github.com/coreos/fleet"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

src_compile() {
	go_build "${COREOS_GO_PACKAGE}/fleetd"
	go_build "${COREOS_GO_PACKAGE}/fleetctl"
}

src_install() {
	dobin ${GOBIN}/*
	dosym ./fleetd /usr/bin/fleet

	systemd_dounit "${FILESDIR}"/${PN}.service
	systemd_dounit "${FILESDIR}"/${PN}.socket
	systemd_dotmpfilesd "${FILESDIR}/tmpfiles.d/${PN}.conf"

	# Grant systemd1 access for fleet user
	insinto /usr/share/polkit-1/rules.d
	doins "${FILESDIR}"/98-fleet-org.freedesktop.systemd1.rules

	# Install sysusers.d snippet which adds fleet group and adds core user into it
	insinto /usr/lib/sysusers.d/
	newins "${FILESDIR}"/sysusers.d/${PN}.conf ${PN}.conf

	coreos-dodoc -r Documentation/*
}
