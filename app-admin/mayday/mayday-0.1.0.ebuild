#
# Copyright (c) 2015 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=5
CROS_WORKON_PROJECT="coreos/mayday"
CROS_WORKON_LOCALNAME="mayday"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/mayday"
inherit coreos-go cros-workon

if [[ "${PV}" == 9999 ]]; then
    KEYWORDS="~amd64 ~arm64"
else
    CROS_WORKON_COMMIT="eaff09119f3a5b911908e4d3200272034466fe5d" # v0.1.0
    KEYWORDS="amd64 arm64"
fi

DESCRIPTION="mayday"
HOMEPAGE="https://github.com/coreos/mayday"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

src_compile() {
	go_build "${COREOS_GO_PACKAGE}/cmd"
}

src_install() {
	newbin ${GOBIN}/cmd mayday
}

