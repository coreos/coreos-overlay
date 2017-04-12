# Copyright 2017 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

inherit autotools versionator

DESCRIPTION="A tiny but valid init for containers"
HOMEPAGE="https://github.com/krallin/tini"
SRC_URI="https://github.com/krallin/${PN}/archive/v${PV}.tar.gz -> ${P}.tar.gz"

LICENSE="MIT"
SLOT="0"
KEYWORDS="amd64 arm64"


src_prepare() {
	for file in configure.ac Makefile.am src/Makefile.am; do
			cp "${FILESDIR}/automake/${file}" "${S}/${file}"
	done
	eapply_user

	export tini_VERSION_MAJOR=$(get_version_component_range 1)
	export tini_VERSION_MINOR=$(get_version_component_range 2)
	export tini_VERSION_PATCH=$(get_version_component_range 3)
	eautoreconf
}
