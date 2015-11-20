EAPI=5

inherit coreos-go eutils git-r3 systemd
COREOS_GO_PACKAGE="github.com/coreos/go-tspi"
EGIT_REPO_URI="git://github.com/coreos/go-tspi.git"

if [[ "${PV}" == 9999 ]]; then
        KEYWORDS="~amd64 ~arm64"
else
        EGIT_COMMIT="9c5928e0350d9829e4d144b461884a259a176dbc"
        KEYWORDS="amd64 arm64"
fi

KEYWORDS="amd64 arm64"
IUSE=""

LICENSE="Apache-2.0"
SLOT="0"

RDEPEND="app-crypt/trousers"
DEPEND="${RDEPEND}"

src_compile() {
	      go_build "${COREOS_GO_PACKAGE}/tpmd"
}

src_install() {
	dobin ${GOBIN}/*
	systemd_dounit "${FILESDIR}"/tpmd.service
	systemd_enable_service multi-user.target tpmd.service
}
