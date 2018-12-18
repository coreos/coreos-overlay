# Copyright 2016 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

# @ECLASS: coreos-go-utils.eclass
# @BLURB: Utility functions for Go
# @DESCRIPTION:
# A utility eclass for functions needed when building both Go itself
# and packages that depend on it. It does not set any metadata.

case "${EAPI:-0}" in
	5|6|7) ;;
	*) die "Unsupported EAPI=${EAPI} for ${ECLASS}"
esac

inherit flag-o-matic toolchain-funcs

# @FUNCTION: go_arch
# @USAGE: [chost]
# @DESCRIPTION:
# export GOARCH=$(go_arch $CHOST)
go_arch() {
	# By chance most portage arch names match Go
	local portage_arch=$(tc-arch "${1:-${CHOST}}")
	local endian=$(tc-endian "${1:-${CHOST}}")
	case "${portage_arch}" in
		x86)	echo 386;;
		x64-*)	echo amd64;;
		ppc64)
			[[ "${endian}" = big ]] && echo ppc64 || echo ppc64le ;;
		*)	echo "${portage_arch}";;
	esac
}

# @FUNCTION: go_arm
# @USAGE: [chost]
# @DESCRIPTION:
# export GOARM=$(go_arm $CHOST)
go_arm() {
	case "${1:-${CHOST}}" in
		armv5*)	echo 5;;
		armv6*)	echo 6;;
		armv7*)	echo 7;;
		*)
			die "unknown GOARM for ${1:-${CHOST}}"
			;;
	esac
}

# @FUNCTION: go_os
# @USAGE: [chost]
# @DESCRIPTION:
# export GOOS=$(go_os $CHOST)
go_os() {
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

# @FUNCTION: go_export
# @USAGE: [chost]
# @DESCRIPTION:
# Export GOARCH, GOOS, GOARM, CC, CXX, and CGO_* environment variables.
go_export() {
	export GOARCH=$(go_arch "$@")
	export GOOS=$(go_os "$@")
	if [[ "${GOARCH}" == "arm" ]]; then
		export GOARM=$(go_arm "$@")
	fi

	# Go's 6l linker does not support PIE, disable so cgo binaries
	# which use 6l+gcc for linking can be built correctly.
	if gcc-specs-pie; then
		append-ldflags -nopie
	fi

	export CC=$(tc-getCC)
	export CXX=$(tc-getCXX)
	export CGO_ENABLED=${CGO_ENABLED:-1}
	export CGO_CFLAGS="${CFLAGS}"
	export CGO_CPPFLAGS="${CPPFLAGS}"
	export CGO_CXXFLAGS="${CXXFLAGS}"
	export CGO_LDFLAGS="${LDFLAGS}"

	# Ensure the `go` wrapper calls the version we expect
	export EGO="${COREOS_GO_VERSION}"
}

# @FUNCTION: go_tuple
# @USAGE: [chost]
# @DESCRIPTION:
# The uniqe string defining a Go target, as used in directory names.
# e.g. linux_amd64 for x86_64-pc-linux-gnu
go_tuple()
{
	echo "$(go_os $@)_$(go_arch $@)"
}

# @FUNCTION: go_cross_compile
# @USAGE: [chost]
# @DESCRIPTION:
# Check if Go consideres compiling for $CHOST as cross-compilation.
# Since Go's target tuples are smaller than GCC's Go may not consider
# itself as a cross-compiler even if the GCC it is using for cgo is.
go_cross_compile()
{
	[[ $(go_tuple ${CBUILD:-${CHOST}}) != $(go_tuple $@) ]]
}
