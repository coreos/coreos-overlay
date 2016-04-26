# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI=6

inherit eutils toolchain-funcs

BOOTSTRAP_DIST="https://dev.gentoo.org/~williamh/dist"
SRC_URI="amd64? ( ${BOOTSTRAP_DIST}/go-linux-amd64-bootstrap.tbz )
	arm64? ( ${BOOTSTRAP_DIST}/go-linux-arm64-bootstrap.tbz )
"

KEYWORDS="-* amd64 arm64"

DESCRIPTION="Version of go compiler used for bootstrapping"
HOMEPAGE="http://www.golang.org"

LICENSE="BSD"
SLOT="0"
IUSE=""

DEPEND=""
RDEPEND=""

# The go tools should not cause the multilib-strict check to fail.
QA_MULTILIB_PATHS="usr/lib/go-bootstrap/pkg/tool/.*/.*"

# The go language uses *.a files which are _NOT_ libraries and should not be
# stripped.
STRIP_MASK="/usr/lib/go-bootstrap/pkg/linux*/*.a
	/usr/lib/go-bootstrap/src/debug/elf/testdata/*
	/usr/lib/go-bootstrap/src/debug/dwarf/testdata/*
	/usr/lib/go-bootstrap/src/runtime/race/*.syso"

go_arch()
{
	# By chance most portage arch names match Go
	local portage_arch=$(tc-arch $@)
	case "${portage_arch}" in
		x86)	echo 386;;
		x64-*)	echo amd64;;
		*)		echo "${portage_arch}";;
	esac
}

go_os()
{
	case "${1:-${CHOST}}" in
		*-linux*)	echo linux;;
		*)
			die "unknown GOOS for ${1:-${CHOST}}"
			;;
	esac
}

S="${WORKDIR}"/go-$(go_os)-$(go_arch)-bootstrap

src_install()
{
	dodir /usr/lib/go-bootstrap
	exeinto /usr/lib/go-bootstrap/bin
	doexe bin/*
	insinto /usr/lib/go-bootstrap
	doins -r lib pkg src
	fperms -R +x /usr/lib/go-bootstrap/pkg/tool
}

pkg_postinst()
{
	# If the go tool sees a package file timestamped older than a dependancy it
	# will rebuild that file.  So, in order to stop go from rebuilding lots of
	# packages for every build we need to fix the timestamps.  The compiler and
	# linker are also checked - so we need to fix them too.
	ebegin "fixing timestamps to avoid unnecessary rebuilds"
	tref="usr/lib/go-bootstrap/pkg/*/runtime.a"
	find "${EROOT}"usr/lib/go-bootstrap -type f \
		-exec touch -r "${EROOT}"${tref} {} \;
	eend $?
}

