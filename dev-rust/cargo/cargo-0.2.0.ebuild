# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

inherit eutils bash-completion-r1

DESCRIPTION="A Rust's package manager"
HOMEPAGE="http://crates.io/"

RUST_SNAPSHOT_DATE="2015-04-04"
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
curl-0.2.7
curl-sys-0.1.20
docopt-0.6.59
env_logger-0.3.0
flate2-0.2.6
gcc-0.3.4
git2-0.2.8
git2-curl-0.2.3
glob-0.2.9
libc-0.1.5
libgit2-sys-0.2.10
libssh2-sys-0.1.17
libz-sys-0.1.2
log-0.3.1
matches-0.1.2
miniz-sys-0.1.4
num_cpus-0.1.0
openssl-sys-0.5.5
pkg-config-0.3.3
regex-0.1.26
rustc-serialize-0.3.12
semver-0.1.19
tar-0.2.9
term-0.2.7
threadpool-0.1.3
time-0.1.24
toml-0.1.20
url-0.2.29
"

SRC_URI="https://github.com/rust-lang/cargo/archive/${PV}.tar.gz -> ${P}.tar.gz
	https://github.com/rust-lang/rust-installer/archive/${RUST_INSTALLER_COMMIT}.tar.gz -> rust-installer-${RUST_INSTALLER_COMMIT}.tar.gz
	$(crate_uris $CRATES)
	x86?   ( https://static-rust-lang-org.s3.amazonaws.com/cargo-dist/${CARGO_SNAPSHOT_DATE}/cargo-nightly-i686-unknown-linux-gnu.tar.gz ->
		cargo-nightly-i686-unknown-linux-gnu-${CARGO_SNAPSHOT_DATE}.tar.gz
		https://static-rust-lang-org.s3.amazonaws.com/dist/${RUST_SNAPSHOT_DATE}/rustc-nightly-i686-unknown-linux-gnu.tar.gz ->
		rustc-nightly-i686-unknown-linux-gnu-${RUST_SNAPSHOT_DATE}.tar.gz )
	amd64? ( https://static-rust-lang-org.s3.amazonaws.com/cargo-dist/${CARGO_SNAPSHOT_DATE}/cargo-nightly-x86_64-unknown-linux-gnu.tar.gz ->
		cargo-nightly-x86_64-unknown-linux-gnu-${CARGO_SNAPSHOT_DATE}.tar.gz
		https://static-rust-lang-org.s3.amazonaws.com/dist/${RUST_SNAPSHOT_DATE}/rustc-nightly-x86_64-unknown-linux-gnu.tar.gz ->
		rustc-nightly-x86_64-unknown-linux-gnu-${RUST_SNAPSHOT_DATE}.tar.gz )"

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
	pushd .. &>/dev/null
	epatch "${FILESDIR}/${P}-local-deps.patch"
	epatch "${FILESDIR}/${P}-makefile.patch"
	popd &>/dev/null
}

src_configure() {
	./configure --prefix="${EPREFIX}"/usr --local-rust-root="${WORKDIR}"/rustc-snapshot/rustc \
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
