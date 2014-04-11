# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_PROJECT="coreos/toolbox"
CROS_WORKON_LOCALNAME="toolbox"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64"
else
	CROS_WORKON_COMMIT="dfadc970444c9f1c0de3062f07f46924ffb12f46"
	KEYWORDS="amd64"
fi

inherit cros-workon

DESCRIPTION="toolbox"
HOMEPAGE="https://github.com/coreos/toolbox"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

src_install() {
	dobin ${S}/toolbox
}
