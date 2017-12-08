# Copyright 2016 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

# @ECLASS: coreos-go-lang.eclass
# @BLURB: Common functionality for building Go itself
# @DESCRIPTION:
# Only dev-lang/go ebuilds should inherit this eclass.
#
# Native (${CHOST} == ${CTARGET}):
# 

case "${EAPI:-0}" in
	6) ;;
	*) die "Unsupported EAPI=${EAPI} for ${ECLASS}"
esac

inherit coreos-go-utils toolchain-funcs versionator

export CBUILD=${CBUILD:-${CHOST}}
export CTARGET=${CTARGET:-${CHOST}}

# Determine the main SLOT we will be using, e.g.: PV=1.5.3 SLOT=1.5
GOSLOT="$(get_version_component_range 1-2)"

DESCRIPTION="A concurrent garbage collected and typesafe programming language"
HOMEPAGE="http://www.golang.org"
SRC_URI="https://storage.googleapis.com/golang/go${PV}.src.tar.gz"

LICENSE="BSD"
SLOT="${GOSLOT}/${PV}"
IUSE=""

RDEPEND="app-eselect/eselect-go"
DEPEND="${RDEPEND}
	>=dev-lang/go-bootstrap-1.5.3"

# These test data objects have writable/executable stacks.
QA_EXECSTACK="usr/lib/go${GOSLOT}/src/debug/elf/testdata/*.obj"

# Similarly, test data is never executed so don't check link dependencies.
REQUIRES_EXCLUDE="/usr/lib/go/src/debug/elf/testdata/*"

# The tools in /usr/lib/go should not cause the multilib-strict check to fail.
QA_MULTILIB_PATHS="usr/lib/go${GOSLOT}/pkg/tool/.*/.*"

# The go language uses *.a files which are _NOT_ libraries and should not be
# stripped. The test data objects should also be left alone and unstripped.
STRIP_MASK="*.a /usr/lib/go${GOSLOT}/src/*"

S="${WORKDIR}/go"

coreos-go-lang_pkg_pretend() {
	# make.bash does not understand cross-compiling a cross-compiler
	if [[ $(go_tuple) != $(go_tuple ${CTARGET}) ]]; then
		die "CHOST CTARGET pair unsupported: CHOST=${CHOST} CTARGET=${CTARGET}"
	fi
}

coreos-go-lang_src_compile() {
	export GOROOT_BOOTSTRAP="${EPREFIX}/usr/lib/go-bootstrap"
	export GOROOT_FINAL="${EPREFIX}/usr/lib/go${GOSLOT}"
	export GOROOT="${S}"
	export GOBIN="${GOROOT}/bin"

	# Go's build script does not use BUILD/HOST/TARGET consistently. :(
	go_export
	export GOHOSTARCH=$(go_arch ${CBUILD})
	export GOHOSTOS=$(go_os ${CBUILD})
	export CC_FOR_TARGET=$(tc-getCC)
	export CXX_FOR_TARGET=$(tc-getCXX)
	# Must be set *after* calling tc-getCC
	export CC=$(tc-getBUILD_CC)

	cd src
	./make.bash || die "build failed"
}

coreos-go-lang_src_test() {
	go_cross_compile && return 0

	cd src
	PATH="${GOBIN}:${PATH}" \
		./run.bash -no-rebuild || die "tests failed"
}

coreos-go-lang_src_install() {
	exeinto "/usr/lib/go${GOSLOT}/bin"
	if go_cross_compile; then
		doexe "${GOBIN}/$(go_tuple)/"{go,gofmt}
	else
		doexe "${GOBIN}/"{go,gofmt}
	fi
	dosym "../lib/go${GOSLOT}/bin/go" "/usr/bin/go${GOSLOT}"
	dosym "../lib/go${GOSLOT}/bin/gofmt" "/usr/bin/gofmt${GOSLOT}"

	exeinto "/usr/lib/go${GOSLOT}/pkg/tool/$(go_tuple)"
	doexe "pkg/tool/$(go_tuple)/"*

	insopts -m0644 -p # preserve timestamps
	insinto "/usr/lib/go${GOSLOT}"
	doins -r doc lib src
	insinto "/usr/lib/go${GOSLOT}/pkg"
	doins -r "pkg/include" "pkg/$(go_tuple)"

	dodoc AUTHORS CONTRIBUTORS PATENTS README.md
}

coreos-go-lang_pkg_postinst() {
	eselect go update
}

coreos-go-lang_pkg_postrm() {
	eselect go update
}

EXPORT_FUNCTIONS pkg_pretend src_compile src_test src_install pkg_postinst pkg_postrm
