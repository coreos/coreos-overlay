# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI=5

export CBUILD=${CBUILD:-${CHOST}}
export CTARGET=${CTARGET:-${CHOST}}
if [[ ${CTARGET} == ${CHOST} ]] ; then
	if [[ ${CATEGORY} == cross-* ]] ; then
		export CTARGET=${CATEGORY#cross-}
	fi
fi

inherit eutils toolchain-funcs

if [[ ${PV} = 9999 ]]; then
	EGIT_REPO_URI="git://github.com/golang/go.git"
	inherit git-r3
else
	SRC_URI="https://storage.googleapis.com/golang/go${PV}.src.tar.gz"
	# arm64 only works when cross-compiling in the SDK
	KEYWORDS="-* ~amd64 ~arm arm64 ~x86 ~amd64-fbsd ~x86-fbsd ~x64-macos ~x86-macos"
fi

DESCRIPTION="A concurrent garbage collected and typesafe programming language"
HOMEPAGE="http://www.golang.org"

LICENSE="BSD"
SLOT="0/${PV}"
IUSE="cros_host"

DEPEND="cros_host? ( >=dev-lang/go-bootstrap-1.4.1 )"
RDEPEND=""

# These test data objects have writable/executable stacks.
QA_EXECSTACK="usr/lib/go/${CTARGET}/src/debug/elf/testdata/*.obj"

# The tools in /usr/lib/go should not cause the multilib-strict check to fail.
QA_MULTILIB_PATHS="usr/lib/go/${CTARGET}/pkg/tool/.*/.*"

# Automatic stripping picks up a *lot* of things that should not be touched.
RESTRICT="strip"

if [[ ${PV} != 9999 ]]; then
	S="${WORKDIR}"/go
fi

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

go_arm()
{
	case "${1:-${CHOST}}" in
		armv5*)	echo 5;;
		armv6*)	echo 6;;
		armv7*)	echo 7;;
		*)
			die "unknown GOARM for ${1:-${CHOST}}"
			;;
	esac
}

go_os()
{
	case "${1:-${CHOST}}" in
		*-linux*)	echo linux;;
		*-darwin*)	echo darwin;;
		*-freebsd*)	echo freebsd;;
		*-netbsd*)	echo netbsd;;
		*-openbsd*)	echo openbsd;;
		*-solaris*)	echo solaris;;
		*-cygwin*|*-interix*|*-winnt*)
			echo windows
			;;
		*)
			die "unknown GOOS for ${1:-${CHOST}}"
			;;
	esac
}

go_tuple()
{
	echo "$(go_os $@)_$(go_arch $@)"
}

go_cross_compile()
{
	[[ $(go_tuple ${CBUILD}) != $(go_tuple) ]]
}

getTARGET_CC()
{
	[[ ${CHOST} == ${CTARGET} ]] && tc-getCC || echo "${CTARGET}-cc"
}

getTARGET_CXX()
{
	[[ ${CHOST} == ${CTARGET} ]] && tc-getCXX || echo "${CTARGET}-c++"
}

pkg_pretend()
{
	# make.bash does not understand cross-compiling a cross-compiler
	if [[ $(go_tuple) != $(go_tuple ${CBUILD}) ]] && \
			[[ $(go_tuple) != $(go_tuple ${CTARGET}) ]]; then
		eerror "Unsupported configuration:"
		eerror " CBUILD:  ${CBUILD}"
		eerror " CHOST:   ${CHOST}"
		eerror " CTARGET: ${CTARGET}"
		die "cross-compiling a cross-compiler"
	fi
}

src_prepare()
{
	epatch_user
}

src_compile()
{
	export GOROOT_BOOTSTRAP="${EPREFIX}"/usr/lib/go1.4
	export GOROOT_FINAL="${EPREFIX}/usr/lib/go/${CTARGET}"
	export GOROOT="$(pwd)"
	export GOBIN="${GOROOT}/bin"

	## Go's build system doesn't differentiate between CBUILD and CHOST
	# Handle CBUILD and CTARGET first, fill in CHOST if needed.
	export GOHOSTARCH=$(go_arch ${CBUILD})
	export GOHOSTOS=$(go_os ${CBUILD})
	export CC=$(tc-getBUILD_CC)

	export GOARCH=$(go_arch ${CTARGET})
	export GOOS=$(go_os ${CTARGET})
	export CC_FOR_TARGET=$(getTARGET_CC)
	export CXX_FOR_TARGET=$(getTARGET_CXX)
	if [[ ${CTARGET} == armv* ]]; then
		export GOARM=$(go_arm ${CTARGET})
	fi

	cd src
	./make.bash || die "build failed"
}

src_test()
{
	go_cross_compile && return 0

	cd src
	PATH="${GOBIN}:${PATH}" \
		./run.bash -no-rebuild || die "tests failed"
}

src_install()
{
	dodir "${GOROOT_FINAL}"
	insinto "${GOROOT_FINAL}"

	rm -r pkg/bootstrap || die
	if go_cross_compile; then
		# CBUILD should not be installed, delete it.
		find bin -type f -maxdepth 1 -delete || die
		rm -r "pkg/$(go_tuple ${CBUILD})" \
			"pkg/tool/$(go_tuple ${CBUILD})" || die
	else
		# keep paths the same regardless of cross-compiling or not
		mkdir "bin/$(go_tuple)" || die
		find bin -type f -maxdepth 1 -exec mv {} "bin/$(go_tuple)" \; || die
	fi

	# There is a known issue which requires the source tree to be installed [1].
	# Once this is fixed, we can consider using the doc use flag to control
	# installing the doc and src directories.
	# [1] https://golang.org/issue/2775
	doins -r bin doc lib pkg src
	fperms -R +x "${GOROOT_FINAL}/bin" "${GOROOT_FINAL}/pkg/tool"

	# selectively strip binaries
	env RESTRICT="" prepstrip \
		"${ED}${GOROOT_FINAL}/bin/$(go_tuple)" \
		"${ED}${GOROOT_FINAL}/pkg/tool/$(go_tuple)"
	if [[ ${CHOST} != ${CTARGET} ]]; then
		env RESTRICT="" CHOST=${CTARGET} prepstrip \
			"${ED}${GOROOT_FINAL}/bin/$(go_tuple ${CTARGET})" \
			"${ED}${GOROOT_FINAL}/pkg/tool/$(go_tuple ${CTARGET})"
	fi

	local x f l
	for x in "bin/$(go_tuple)"/*; do
		f=${x##*/}
		l="..${GOROOT_FINAL#/usr}/bin/${f}"
		dosym "${l}" /usr/bin/${CTARGET}-${f}
		if [[ ${CHOST} == ${CTARGET} ]]; then
			dosym "${l}" /usr/bin/${f}
		fi
	done

	dodir "${GOROOT_FINAL}/misc"
	insinto "${GOROOT_FINAL}/misc"
	doins -r misc/trace

	# only for native packages to avoid conflicts
	if [[ ${CHOST} == ${CTARGET} ]]; then
		dodoc AUTHORS CONTRIBUTORS PATENTS README.md
	fi
}

pkg_preinst()
{
	has_version '<dev-lang/go-1.4' &&
		export had_support_files=true ||
		export had_support_files=false
}

pkg_postinst()
{
	# If the go tool sees a package file timestamped older than a dependancy it
	# will rebuild that file.  So, in order to stop go from rebuilding lots of
	# packages for every build we need to fix the timestamps.  The compiler and
	# linker are also checked - so we need to fix them too.
	ebegin "fixing timestamps to avoid unnecessary rebuilds"
	tref="usr/lib/go/${CTARGET}/pkg/*/runtime.a"
	find "${EROOT}usr/lib/go/${CTARGET}" -type f \
		-exec touch -r "${EROOT}"${tref} {} \;
	eend $?

	if [[ ${PV} != 9999 && -n ${REPLACING_VERSIONS} &&
		${REPLACING_VERSIONS} != ${PV} ]]; then
		elog "Release notes are located at http://golang.org/doc/go${PV}"
	fi

	if $had_support_files; then
		ewarn
		ewarn "All editor support, IDE support, shell completion"
		ewarn "support, etc has been removed from the go package"
		ewarn "upstream."
		ewarn "For more information on which support is available, see"
		ewarn "the following URL:"
		ewarn "https://github.com/golang/go/wiki/IDEsAndTextEditorPlugins"
	fi
}
