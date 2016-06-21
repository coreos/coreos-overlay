# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	KEYWORDS="amd64 arm64"
fi

inherit systemd

DESCRIPTION="flannel"
HOMEPAGE="https://github.com/coreos/flannel"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

RDEPEND="app-admin/sdnotify-proxy"
S="$WORKDIR"

src_install() {
	case ${ARCH} in
		amd64) flannel_img="quay.io/coreos/flannel" ;;
		arm64) flannel_img="quay.io/coreos/arm64-flannel" ;;
		*) die "unsupported arch: ${ARCH}" ;;
	esac

	cp "${FILESDIR}"/flanneld.service "${T}"/flanneld.service
	sed --in-place "s|{{flannel_ver}}|${PV}|" "${T}"/flanneld.service
	sed --in-place "s|{{flannel_img}}|${flannel_img}|" "${T}"/flanneld.service

	systemd_dounit "${T}"/flanneld.service

	insinto /usr/lib/systemd/network
	doins "${FILESDIR}"/50-flannel.network
}
