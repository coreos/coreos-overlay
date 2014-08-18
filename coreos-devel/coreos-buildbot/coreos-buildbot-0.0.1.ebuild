# Copyright 2013 The CoreOS Authors.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
PYTHON_COMPAT=( python2_7 )

CROS_WORKON_COMMIT="3e4b20f67839aa541839eca6b4b7274d5ad1932c"
CROS_WORKON_PROJECT="coreos/coreos-buildbot"
CROS_WORKON_REPO="git://github.com"

inherit cros-workon distutils-r1

DESCRIPTION="Tools and modules for CoreOS BuildBots"
HOMEPAGE="https://github.com/coreos/coreos-buildbot"
SRC_URI=""

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

RDEPEND="${PYTHON_DEPS}
	dev-python/pyflakes[${PYTHON_USEDEP}]
	dev-python/python-dateutil[${PYTHON_USEDEP}]
	dev-python/twisted-core
	dev-util/buildbot
	"
DEPEND="${RDEPEND}"

DOCS=( README.md )

python_test() {
	# Note: Current stable versions of twisted don't use the python-r1 eclass
	# but there is some special magic in the trial wrapper to use the right
	# python version based on EPYTHON which is exported by python-r1.
	trial coreos || die
}
