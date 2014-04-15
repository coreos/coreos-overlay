# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

inherit systemd versionator

DESCRIPTION="Fake data for XenServer's PV driver version detection."
HOMEPAGE="http://xenserver.org/"
SRC_URI=""

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

DEPEND=""
RDEPEND="app-emulation/xenstore"

S="${WORKDIR}"

src_prepare() {
	local split=($(get_version_components))
	sed -e "s/@@MAJOR@@/${split[0]}/" \
		-e "s/@@MINOR@@/${split[1]}/" \
		-e "s/@@MICRO@@/${split[2]}/" \
		"${FILESDIR}"/xenserver-pv-version.service \
		> "${T}"/xenserver-pv-version.service || die
}

src_install() {
	systemd_dounit "${T}"/xenserver-pv-version.service
	systemd_enable_service sysinit.target xenserver-pv-version.service
}
