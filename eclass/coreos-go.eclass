# Copyright 2014 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2
# $Header: $

# @ECLASS: coreos-go.eclass
# @BLURB: utility functions for building Go binaries

# @ECLASS-VARIABLE: COREOS_GO_PACKAGE
# @DESCRIPTION:
# Name of the Go package unpacked to ${S}, this is required.

[[ ${EAPI} != "5" ]] && die "${ECLASS}: Only EAPI=5 is supported"

inherit flag-o-matic multiprocessing toolchain-funcs

DEPEND="dev-lang/go"

# @FUNCTION: go_get_arch
# @USAGE: export GOARCH=$(go_get_arch)
go_get_arch() {
	echo ${ARCH}
}

# @FUNCTION: go_build
# @USAGE: <package-name> [<binary-name>]
go_build() {
	debug-print-function ${FUNCNAME} "$@"

	[[ $# -eq 0 || $# -gt 2 ]] && die "${ECLASS}: go_install: incorrect # of arguments"
	local package_name="$1"
	local binary_name="${package_name##*/}"

	go build -x -p "$(makeopts_jobs)" \
		-ldflags "${GO_LDFLAGS} -extldflags '${LDFLAGS}'" \
		-o "${GOPATH}/bin/${binary_name}" "${package_name}" \
		|| die "${ECLASS}: go build failed"
}

coreos-go_src_prepare() {
	debug-print-function ${FUNCNAME} "$@"

	export GOARCH=$(go_get_arch)
	export GOPATH="${WORKDIR}/gopath"
	export GOBIN="${GOPATH}/bin"
	mkdir -p "${GOBIN}" || die "${ECLASS}: bad path: ${GOBIN}"

	if [[ -z "${COREOS_GO_PACKAGE}" ]]; then
		die "${ECLASS}: COREOS_GO_PACKAGE must be defined by the ebuild"
	fi

	local package_path="${GOPATH}/src/${COREOS_GO_PACKAGE}"
	mkdir -p "${package_path%/*}" || die "${ECLASS}: bad path: ${package_path%/*}"
	ln -sT "${S}" "${package_path}" || die "${ECLASS}: bad path: ${S}"

	# Go's 6l linker does not support PIE, disable so cgo binaries
	# which use 6l+gcc for linking can be built correctly.
	if gcc-specs-pie; then
		append-ldflags -nopie
	fi

	export CC=$(tc-getCC)
	export CGO_ENABLED=1
	export CGO_CFLAGS="${CFLAGS}"
	export CGO_CPPFLAGS="${CPPFLAGS}"
	export CGO_CXXFLAGS="${CXXFLAGS}"
	export CGO_LDFLAGS="${LDFLAGS}"
}

coreos-go_src_compile() {
	debug-print-function ${FUNCNAME} "$@"

	go_build "${COREOS_GO_PACKAGE}"
}

coreos-go_src_install() {
	debug-print-function ${FUNCNAME} "$@"

	dobin "${GOBIN}"/*
}

EXPORT_FUNCTIONS src_prepare src_compile src_install
