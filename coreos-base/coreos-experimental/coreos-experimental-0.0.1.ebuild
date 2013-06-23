# Copyright 1999-2013 CoreOS Inc
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=4

DESCRIPTION="CoreOS Experimental Packages"
HOMEPAGE="http://coreos.com"

LICENSE="GPL-@"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

DEPEND=""
RDEPEND="
	app-admin/systemd-rest
	app-admin/etcd
	app-admin/etcd-client
	app-admin/etcd-lib
"
