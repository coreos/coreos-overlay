# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-libs/protobuf/protobuf-2.2.0.ebuild,v 1.1 2009/08/21 17:41:18 nelchael Exp $

EAPI="2"

JAVA_PKG_IUSE="source"

inherit eutils python java-pkg-opt-2 elisp-common

DESCRIPTION="Google's Protocol Buffers -- an efficient method of encoding structured data"
HOMEPAGE="http://code.google.com/p/protobuf/"
SRC_URI="http://protobuf.googlecode.com/files/${PF}.tar.bz2"

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="~amd64 ~x86 ~arm"
IUSE="emacs examples java python vim-syntax"

DEPEND="${DEPEND} java? ( >=virtual/jdk-1.5 )
	python? ( dev-python/setuptools )
	emacs? ( virtual/emacs )"
RDEPEND="${RDEPEND} java? ( >=virtual/jre-1.5 )
	emacs? ( virtual/emacs )"

src_prepare() {
	epatch "${FILESDIR}/${P}-decoder_test_64bit_fix.patch"
	epatch "${FILESDIR}/${P}-fix-emacs-byte-compile.patch"
}

src_configure() {
	local protoc=""
	if tc-is-cross-compiler; then
		protoc="--with-protoc=$(which protoc)" ||
			die "need native protoc tool to cross compile"
	fi
	econf ${protoc} || die "died running econf"
}

src_compile() {
	emake || die

	if use python; then
		cd python; distutils_src_compile; cd ..
	fi

	if use java; then
		src/protoc --java_out=java/src/main/java --proto_path=src src/google/protobuf/descriptor.proto
		mkdir java/build
		pushd java/src/main/java
		ejavac -d ../../../build $(find . -name '*.java') || die "java compilation failed"
		popd
		jar cf "${PN}.jar" -C java/build . || die "jar failed"
	fi

	if use emacs; then
		elisp-compile "${S}/editors/protobuf-mode.el" || die "elisp-compile failed!"
	fi
}

src_install() {
	emake DESTDIR="${D}" install
	dodoc CHANGES.txt CONTRIBUTORS.txt README.txt

	if use python; then
		cd python; distutils_src_install; cd ..
	fi

	if use vim-syntax; then
		insinto /usr/share/vim/vimfiles/syntax
		doins editors/proto.vim
	fi

	if use emacs; then
		elisp-install ${PN} editors/protobuf-mode.el* || die "elisp-install failed!"
		elisp-site-file-install "${FILESDIR}/70${PN}-gentoo.el"
	fi

	if use examples; then
		insinto /usr/share/doc/${PF}/examples
		doins -r examples/* || die "doins examples failed"
	fi

	if use java; then
		java-pkg_dojar ${PN}.jar
		use source && java-pkg_dosrc java/src/main/java/*
	fi
}

src_test() {
	emake check

	if use python; then
		 cd python; ${python} setup.py test || die "python test failed"
		 cd ..
	fi
}

pkg_postinst() {
	use emacs && elisp-site-regen
}

pkg_postrm() {
	use emacs && elisp-site-regen
}
