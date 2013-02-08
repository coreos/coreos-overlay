# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4

DESCRIPTION="Chromium OS CA Certificates PEM files"
HOMEPAGE="http://src.chromium.org"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

# This package cannot co-exist in the build target with
# app-misc/ca-certificates because of file conflicts.  Moreover,
# this package is a replacement for ca-certificates, so generally
# the two packages should not co-exist in any event.
#
# For maxiumum confusion, we depend on app-misc/ca-certificates from
# the build host for the "update-ca-certificates" script.  That
# dependency must be specified in chromeos-base/hard-host-depends,
# as there's no way with Portage to specify that dependency here (as
# of this writing, at any rate).
RDEPEND="!app-misc/ca-certificates"
DEPEND="$RDEPEND
	dev-libs/openssl"

# Because this ebuild has no source package, "${S}" doesn't get
# automatically created.  The compile phase depends on "${S}" to
# exist, so we make sure "${S}" refers to a real directory.
#
# The problem is apparently an undocumented feature of EAPI 4;
# earlier versions of EAPI don't require this.
S="${WORKDIR}"

# N.B.  The cert files are in ${FILESDIR}, not a separate source
# code repo.  If you add or delete a cert file, you'll need to bump
# the revision number for this ebuild manually.
src_install() {
	insinto /usr/share/ca-certificates
	doins "${FILESDIR}"/chromeos-certs/*.crt

	# Create required inputs to the update-ca-certificates script.
	dodir /etc/ssl/certs
	dodir /etc/ca-certificates/update.d
	(
		cd "${D}"/usr/share/ca-certificates
		find * -name '*.crt' | sort
	) > "${D}"/etc/ca-certificates.conf

	update-ca-certificates --root "${D}"
}
