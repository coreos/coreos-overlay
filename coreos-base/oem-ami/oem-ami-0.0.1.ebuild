# Copyright (c) 2013 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

DESCRIPTION="OEM suite for AMI images"
HOMEPAGE=""
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE=""

# no source directory
S="${WORKDIR}"

src_install() {
	into "/"
	dobin ${FILESDIR}/ec2-ssh-key
	dobin ${FILESDIR}/coreos-setup-environment
	dobin ${FILESDIR}/coreos-c10n
	dobin ${FILESDIR}/etcd-bootstrap

	insinto "/"
	doins ${FILESDIR}/cloud-config.yml
}
