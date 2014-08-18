# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI="5"
PYTHON_COMPAT=( python2_7 )

inherit distutils-r1

DESCRIPTION="command line tool for interacting with cloud storage services"
HOMEPAGE="https://github.com/GoogleCloudPlatform/gsutil"
SRC_URI="http://commondatastorage.googleapis.com/pub/${PN}_${PV}.tar.gz"

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

DEPEND="${PYTHON_DEPS}"
RDEPEND="${DEPEND}
	>=dev-python/boto-2.9.7[${PYTHON_USEDEP}]
	>=dev-python/crcmod-1.7
	>=dev-python/httplib2-0.8[${PYTHON_USEDEP}]
	>=dev-python/pyopenssl-0.13[${PYTHON_USEDEP}]
	dev-python/google-api-python-client[${PYTHON_USEDEP}]
	dev-python/python-gflags[${PYTHON_USEDEP}]
	dev-python/setuptools[${PYTHON_USEDEP}]
	dev-python/socksipy-branch[${PYTHON_USEDEP}]
	"

S=${WORKDIR}/${PN}
DOCS=( README.md CHANGES.md )
PATCHES=(
	${FILESDIR}/${P}-use-friendy-version-checks.patch
	${FILESDIR}/${P}-Fix-update-tests-that-fail-for-package-installs.patch
	${FILESDIR}/${P}-Fix-test-using-glob-with-ObjectToURI.patch
)

python_test() {
	export BOTO_CONFIG=${FILESDIR}/dummy.boto
	${PYTHON} gslib/__main__.py test -u || die "tests failed"
}
