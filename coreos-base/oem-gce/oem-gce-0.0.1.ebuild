# Copyright (c) 2013 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

DESCRIPTION="OEM suite for Google Compute Engine images"
HOMEPAGE=""
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

# no source directory
S="${WORKDIR}"

RDEPEND="dev-lang/python"

src_install() {
	into "/"
	dobin ${FILESDIR}/gce-ssh-key

	insinto "/"
	doins ${FILESDIR}/cloud-config.yml
}
