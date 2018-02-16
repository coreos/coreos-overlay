# Copyright 2014 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

# @ECLASS: coreos-go.eclass
# @BLURB: utility functions for building Go binaries

# @ECLASS-VARIABLE: COREOS_GO_PACKAGE
# @REQUIRED
# @DESCRIPTION:
# Name of the Go package unpacked to ${S}.
#
# Example:
# @CODE
# COREOS_GO_PACKAGE="github.com/coreos/mantle"
# @CODE

# @ECLASS-VARIABLE: COREOS_GO_VERSION
# @DESCRIPTION:
# This variable specifies the version of Go to use. If ommitted the
# default value from coreos-go-depend.eclass will be used.
#
# Example:
# @CODE
# COREOS_GO_VERSION=go1.10
# @CODE

case "${EAPI:-0}" in
	5|6) ;;
	*) die "Unsupported EAPI=${EAPI} for ${ECLASS}"
esac

inherit coreos-go-depend multiprocessing

# @FUNCTION: go_build
# @USAGE: <package-name> [<binary-name>]
go_build() {
	debug-print-function ${FUNCNAME} "$@"

	[[ $# -eq 0 || $# -gt 2 ]] && die "${ECLASS}: ${FUNCNAME}: incorrect # of arguments"
	local package_name="$1"
	local binary_name="${package_name##*/}"

	ebegin "${EGO} build ${package_name}"
	debug-print EGO=${EGO} $(${EGO} env)

	${EGO} build -v \
		-p "$(makeopts_jobs)" \
		-ldflags "${GO_LDFLAGS} -extldflags '${LDFLAGS}'" \
		-o "${GOBIN}/${binary_name}" \
		"${package_name}"

	local e=${?}
	local msg="${FUNCNAME}: ${package_name} failed (${e})."
	eend ${e} "${msg}" || die "${msg}"
}

coreos-go_src_prepare() {
	debug-print-function ${FUNCNAME} "$@"
	has ${EAPI:-0} 6 && default

	go_export
	export GOPATH="${WORKDIR}/gopath"
	export GOBIN="${GOPATH}/bin"

	mkdir -p "${GOBIN}" || die "${ECLASS}: bad path: ${GOBIN}"

	if [[ -z "${COREOS_GO_PACKAGE}" ]]; then
		die "${ECLASS}: COREOS_GO_PACKAGE must be defined by the ebuild"
	fi

	local package_path="${GOPATH}/src/${COREOS_GO_PACKAGE}"
	mkdir -p "${package_path%/*}" || die "${ECLASS}: bad path: ${package_path%/*}"
	ln -sT "${S}" "${package_path}" || die "${ECLASS}: bad path: ${S}"
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
