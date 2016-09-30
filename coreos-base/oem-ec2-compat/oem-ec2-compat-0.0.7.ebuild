# Copyright (c) 2013 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

DESCRIPTION="OEM suite for EC2 compatible images"
HOMEPAGE=""
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64 arm64 x86"
IUSE="ec2 openstack brightbox"
REQUIRED_USE="^^ ( ec2 openstack brightbox )"

# no source directory
S="${WORKDIR}"

src_prepare() {
	if use ec2 ; then
		ID="ami"
		NAME="Amazon EC2"
		HOME_URL="http://aws.amazon.com/ec2/"
	elif use openstack ; then
		ID="openstack"
		NAME="Openstack"
		HOME_URL="https://www.openstack.org/"
	elif use brightbox ; then
		ID="brightbox"
		NAME="Brightbox"
		HOME_URL="http://brightbox.com/"
	else
		die "Unknown OEM!"
	fi

	sed -e "s\\@@OEM_ID@@\\${ID}\\g" \
	    -e "s\\@@OEM_NAME@@\\${NAME}\\g" \
	    -e "s\\@@OEM_VERSION_ID@@\\${PVR}\\g" \
	    -e "s\\@@OEM_HOME_URL@@\\${HOME_URL}\\g" \
	    "${FILESDIR}/cloud-config.yml" > "${T}/cloud-config.yml" || die
}

src_install() {
	insinto "/usr/share/oem"
	doins "${T}/cloud-config.yml"
	if use ec2 ; then
		newins "${FILESDIR}/grub-ec2.cfg" grub.cfg
	elif use openstack ; then
		newins "${FILESDIR}/grub-openstack.cfg" grub.cfg
	elif use brightbox ; then
		newins "${FILESDIR}/grub-brightbox.cfg" grub.cfg
	fi
}
