# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

inherit eutils bash-completion-r1

DESCRIPTION="A Rust's package manager"
HOMEPAGE="http://crates.io/"

CARGO_SNAPSHOT_DATE="2015-04-02"
RUST_INSTALLER_COMMIT="e54d4823d26cdb3f98e5a1b17e1c257cd329aa61"

crate_uris(){
	while (( "$#" )); do
		local name version url
		name="${1%-*}"
		version="${1##*-}"
		url="https://crates.io/api/v1/crates/${name}/${version}/download -> ${1}.crate"
		echo $url
		shift
	done
}

CRATES="bitflags-0.1.1
curl-0.2.10
curl-sys-0.1.24
docopt-0.6.67
env_logger-0.3.1
filetime-0.1.4
flate2-0.2.7
gcc-0.3.8
git2-0.2.11
git2-curl-0.2.4
glob-0.2.10
libc-0.1.8
libgit2-sys-0.2.17
libssh2-sys-0.1.25
libz-sys-0.1.6
log-0.3.1
matches-0.1.2
miniz-sys-0.1.5
num_cpus-0.2.6
openssl-sys-0.6.2
pkg-config-0.3.4
regex-0.1.30
rustc-serialize-0.3.14
semver-0.1.19
strsim-0.3.0
tar-0.2.14
term-0.2.9
threadpool-0.1.4
time-0.1.26
toml-0.1.21
url-0.2.35
"

SRC_URI="https://github.com/rust-lang/cargo/archive/${PV}.tar.gz -> ${P}.tar.gz
	https://github.com/rust-lang/rust-installer/archive/${RUST_INSTALLER_COMMIT}.tar.gz -> rust-installer-${RUST_INSTALLER_COMMIT}.tar.gz
	$(crate_uris $CRATES)
	x86?   ( https://static-rust-lang-org.s3.amazonaws.com/cargo-dist/${CARGO_SNAPSHOT_DATE}/cargo-nightly-i686-unknown-linux-gnu.tar.gz ->
		cargo-nightly-i686-unknown-linux-gnu-${CARGO_SNAPSHOT_DATE}.tar.gz )
	amd64? ( https://static-rust-lang-org.s3.amazonaws.com/cargo-dist/${CARGO_SNAPSHOT_DATE}/cargo-nightly-x86_64-unknown-linux-gnu.tar.gz ->
		cargo-nightly-x86_64-unknown-linux-gnu-${CARGO_SNAPSHOT_DATE}.tar.gz )"

LICENSE="|| ( MIT Apache-2.0 )"
SLOT="0"
KEYWORDS="~amd64 ~x86"

IUSE=""

COMMON_DEPEND="sys-libs/zlib
	dev-libs/openssl:*
	net-libs/libssh2
	net-libs/http-parser"
RDEPEND="${COMMON_DEPEND}
	net-misc/curl[ssl]"
DEPEND="${COMMON_DEPEND}
	|| ( >=dev-lang/rust-1.1.0 >=dev-lang/rust-bin-1.1.0 )
	dev-util/cmake"

src_unpack() {
	for archive in ${A}; do
		case "${archive}" in
			*.crate)
				ebegin "Unpacking ${archive}"
				tar -xf "${DISTDIR}"/${archive} || die
				eend $?
				;;
			*)
				unpack ${archive}
				;;
		esac
	done
	mv rustc-nightly-*-unknown-linux-gnu "rustc-snapshot"
	mv cargo-nightly-*-unknown-linux-gnu "cargo-snapshot"
	mv "rust-installer-${RUST_INSTALLER_COMMIT}"/* "${P}"/src/rust-installer
}

src_prepare() {
	epatch "${FILESDIR}/${P}-makefile.patch"
	pushd .. &>/dev/null
	epatch "${FILESDIR}/${P}-local-deps.patch"
	popd &>/dev/null
}

src_configure() {
	./configure --prefix="${EPREFIX}"/usr \
		--disable-verify-install --disable-debug --enable-optimize \
		--local-cargo="${WORKDIR}"/cargo-snapshot/cargo/bin/cargo || die
}

src_compile() {
	emake VERBOSE=1 PKG_CONFIG_PATH="" || die
}

src_install() {
	CFG_DISABLE_LDCONFIG="true" emake DESTDIR="${D}" install || die
	dobashcomp "${ED}"/usr/etc/bash_completion.d/cargo
	rm -rf "${ED}"/usr/etc
}
