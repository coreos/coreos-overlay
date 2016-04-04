# Copyright 1999-2013 Gentoo Foundation
# Copyright 2016 CoreOS, Inc
# Distributed under the terms of the GNU General Public License v2

EAPI="4"

DESCRIPTION="Tools for generating TPM policy"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 x86 arm64"
IUSE=""

S="${WORKDIR}"

src_install() {
    dosbin "${FILESDIR}"/tpm_hostpolicy
}
