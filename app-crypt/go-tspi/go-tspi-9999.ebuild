EAPI=5

inherit coreos-go eutils git-r3 systemd
COREOS_GO_PACKAGE="github.com/coreos/go-tspi"
EGIT_REPO_URI="git://github.com/coreos/go-tspi.git"

if [[ "${PV}" == 9999 ]]; then
        KEYWORDS="~amd64 ~arm64"
else
        EGIT_COMMIT="1708d740a1fec08db39d9c15960ed6cc3a7e974a"
        KEYWORDS="amd64 arm64"
fi

IUSE=""

LICENSE="Apache-2.0"
SLOT="0"

RDEPEND="app-crypt/trousers"
DEPEND="${RDEPEND}"

src_compile() {
	      go_build "${COREOS_GO_PACKAGE}/tpmd"
      	      go_build "${COREOS_GO_PACKAGE}/tpmown"
}

src_install() {
	dobin ${GOBIN}/*
	systemd_dounit "${FILESDIR}"/tpmd.service
}
