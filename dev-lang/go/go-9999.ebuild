# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header:  $

EAPI=5

inherit eutils toolchain-funcs

EGIT_REPO_URI="git://github.com/golang/go.git"
inherit git-r3
KEYWORDS="-* ~amd64 arm64"

DESCRIPTION="A concurrent garbage collected and typesafe programming language"
HOMEPAGE="http://www.golang.org"

LICENSE="BSD"
SLOT="0"
IUSE=""

DEPEND=""
RDEPEND=""

# The tools in /usr/lib/go should not cause the multilib-strict check to fail.
QA_MULTILIB_PATHS="usr/lib/go/pkg/tool/.*/.*"

# The go language uses *.a files which are _NOT_ libraries and should not be
# stripped.
STRIP_MASK="/usr/lib/go/pkg/linux*/*.a /usr/lib/go/pkg/freebsd*/*.a /usr/lib/go/pkg/darwin*/*.a"

build_arch()
{
    case "$CBUILD" in
        aarch64*)   echo arm64;;
        x86_64*)    echo amd64;;
    esac
}

same_arch()
{
	[[ "${ARCH}" = "$(build_arch)" ]]
}

src_prepare()
{

	epatch_user
}

src_compile()
{
	export GOOS="linux"
	export GOARCH="${ARCH}"
	export GOROOT_BOOTSTRAP="/usr/lib/go1.4"
	export GOROOT_FINAL="${EPREFIX}"/usr/lib/go
	export GOROOT="$(pwd)"
	export GOBIN=${GOROOT}/bin

	cd src
	./make.bash || die "build failed"
}

src_test()
{
	$(same_arch) || return 0;

	cd src
	PATH="${GOBIN}:${PATH}" \
		./run.bash --no-rebuild --banner || die "tests failed"
}

src_install()
{
	local bin_path

	if $(same_arch); then
		bin_path=${GOBIN}
	else
		bin_path=${GOBIN}/${GOOS}_${GOARCH}
	fi

	dobin ${bin_path}/*
	dodoc AUTHORS CONTRIBUTORS PATENTS README.md

	dodir /usr/lib/go
	insinto /usr/lib/go

	# There is a known issue which requires the source tree to be installed [1].
	# Once this is fixed, we can consider using the doc use flag to control
	# installing the doc and src directories.
	# [1] https://golang.org/issue/2775
	doins -r doc lib pkg src
	fperms -R +x /usr/lib/go/pkg/tool
}

pkg_postinst()
{
	# If the go tool sees a package file timestamped older than a dependancy it
	# will rebuild that file.  So, in order to stop go from rebuilding lots of
	# packages for every build we need to fix the timestamps.  The compiler and
	# linker are also checked - so we need to fix them too.
	ebegin "fixing timestamps to avoid unnecessary rebuilds"
	tref="usr/lib/go/pkg/*/runtime.a"
	find "${EROOT}"usr/lib/go -type f \
		-exec touch -r "${EROOT}"${tref} {} \;
	eend $?
}
