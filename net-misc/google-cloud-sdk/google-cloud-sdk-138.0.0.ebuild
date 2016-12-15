# Copyright 1999-2016 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI=6

PYTHON_COMPAT=( python2_7 )

inherit bash-completion-r1 python-single-r1

DESCRIPTION="Command line tool for interacting with Google Compute Engine"
HOMEPAGE="https://cloud.google.com/sdk/#linux"
SRC_URI="https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/${P}-linux-x86_64.tar.gz"

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

S="${WORKDIR}/${PN}"

DEPEND="${PYTHON_DEPS}"
RDEPEND="${DEPEND}
	dev-python/crcmod[${PYTHON_USEDEP}]
	!net-misc/gsutil"

src_prepare() {
	# Drop unnused python3 code
	rm -r platform/gsutil/third_party/httplib2/python3 || die
	# Use the compiled crcmod from the system
	rm -r platform/gsutil/third_party/{crcmod,crcmod_osx} || die

	default
}

src_install() {
	insinto "/usr/lib/${PN}"
	doins -r lib platform "${FILESDIR}/properties"

	python_optimize "${D}/usr/lib/${PN}"

	dobin "${FILESDIR}/"{gcloud,gsutil}
	dodoc LICENSE README RELEASE_NOTES
	doman help/man/man1/*.1

	newbashcomp completion.bash.inc gcloud
	bashcomp_alias gcloud gsutil
}
