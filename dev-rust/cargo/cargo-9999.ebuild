# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

inherit eutils bash-completion-r1 git-r3

DESCRIPTION="A Rust's package manager"
HOMEPAGE="http://crates.io/"

LICENSE="|| ( MIT Apache-2.0 )"
SLOT="0"
KEYWORDS=""

IUSE=""

EGIT_REPO_URI="https://github.com/rust-lang/cargo.git"

COMMON_DEPEND=">=virtual/rust-999
	sys-libs/zlib
	dev-libs/openssl:*
	net-libs/libssh2
	net-libs/http-parser"
RDEPEND="${COMMON_DEPEND}
	net-misc/curl[ssl]"
DEPEND="${COMMON_DEPEND}
	dev-util/cmake"

src_configure() {
	./configure --prefix="${EPREFIX}"/usr --disable-verify-install || die
}

src_compile() {
	emake VERBOSE=1 PKG_CONFIG_PATH="" || die
}

src_install() {
	CFG_DISABLE_LDCONFIG="true" emake DESTDIR="${D}" install || die
	dobashcomp "${ED}"/usr/etc/bash_completion.d/cargo
	rm -rf "${ED}"/usr/etc
}
