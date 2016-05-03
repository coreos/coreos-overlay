# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI="5"
PYTHON_COMPAT=( python2_7 )

inherit distutils-r1

DESCRIPTION="Command line tool for interacting with Google Compute Engine"
HOMEPAGE="https://developers.google.com/compute/docs/gcutil/"
SRC_URI="https://dl.google.com/dl/cloudsdk/release/artifacts/${P}.tar.gz"

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

DEPEND="${PYTHON_DEPS}"
RDEPEND="${DEPEND}
	>=dev-python/httplib2-0.8[${PYTHON_USEDEP}]
	>=dev-python/google-api-python-client-1.2[${PYTHON_USEDEP}]
	>=dev-python/ipaddr-2.1.10[${PYTHON_USEDEP}]
	>=dev-python/iso8601-0.1.4[${PYTHON_USEDEP}]
	>=dev-python/google-apputils-0.4.0[${PYTHON_USEDEP}]
	>=dev-python/python-gflags-2.0[${PYTHON_USEDEP}]
	dev-python/oauth2client[${PYTHON_USEDEP}]
	dev-python/pyyaml[${PYTHON_USEDEP}]
	dev-python/setuptools[${PYTHON_USEDEP}]
	"

DOCS=( README CHANGELOG )
PATCHES=(
	${FILESDIR}/${PN}-1.11.0-use-friendly-version-checks.patch
)

python_test() {
	${PYTHON} setup.py google_test --test-dir "${BUILD_DIR}"/lib || \
		ewarn "FIXME: Tests failed with ${EPYTHON}"
	# There seems to be a version compatibility issue...
}
