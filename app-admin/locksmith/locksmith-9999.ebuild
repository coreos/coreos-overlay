# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_PROJECT="coreos/locksmith"
CROS_WORKON_LOCALNAME="locksmith"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64"
else
	CROS_WORKON_COMMIT="c50d9939f9c08af8b0a2a615003471df3a4e79bc" # v0.1.9 tag
	KEYWORDS="amd64"
fi

inherit cros-workon systemd

DESCRIPTION="locksmith"
HOMEPAGE="https://github.com/coreos/locksmith"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND=">=dev-lang/go-1.2"

src_compile() {
	# work around gentoo hardened gcc incompatibilities with cgo
	# see https://bugs.gentoo.org/show_bug.cgi?id=493328
	if gcc-specs-pie; then
		GOLDFLAGS="-extldflags -fno-PIC"
	fi

	GOLDFLAGS=${GOLDFLAGS} ./build || die
}

src_install() {
	dobin ${S}/bin/locksmithctl
	dodir /usr/lib/locksmith
	dosym ../../../bin/locksmithctl /usr/lib/locksmith/locksmithd

	systemd_dounit "${S}"/systemd/locksmithd.service
	systemd_enable_service multi-user.target locksmithd.service
}
