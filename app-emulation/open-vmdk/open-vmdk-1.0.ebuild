# Copyright 2014 VMware
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

inherit git-2

DESCRIPTION="Tool to convert vmdk to an ova file"
HOMEPAGE="https://github.com/vmware/open-vmdk"
LICENSE="Apache-2.0"
SLOT="0"

EGIT_REPO_URI="https://github.com/vmware/open-vmdk"
EGIT_BRANCH="master"
EGIT_COMMIT="82eb7268e78cc32907573b713569e1331c571ce5"

KEYWORDS="amd64 ~x86 ppc64"
IUSE=""

DEPEND=""
RDEPEND="app-misc/jq ${DEPEND}"

src_install() {
	emake DESTDIR="${D}" install
}
