#
# $Header:$
#

EAPI=4
CROS_WORKON_COMMIT="000fa3a301fdfd04b3fe34e3a761285e5f7d12dd"
CROS_WORKON_PROJECT="coreos/coretest"
CROS_WORKON_LOCALNAME="coretest"
CROS_WORKON_REPO="git://github.com"
inherit cros-workon

DESCRIPTION="Sanity tests for CoreOS"
HOMEPAGE="https://github.com/coreos/coretest"
SRC_URI=""

LICENSE="APACHE"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

DEPEND=">=dev-lang/go-1.1"

src_compile() {
	./build
}

src_install() {
	dobin "${S}/${PN}"
}
