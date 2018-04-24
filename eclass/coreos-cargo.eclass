# Copyright 2017-2018 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

# @ECLASS: coreos-cargo.eclass
# @MAINTAINER:
# team-os@coreos.com
# @AUTHOR:
# David Michael <david.michael@coreos.com>
# @BLURB: cargo cross-compilation support for CoreOS/ChromeOS targets

if [[ -z ${_COREOS_CARGO_ECLASS} ]]; then
_COREOS_CARGO_ECLASS=1

# XXX: Don't require host dependencies to also be in the sysroot.
CATEGORY=dev-util PN=cargo inherit cargo
inherit toolchain-funcs

EXPORT_FUNCTIONS src_unpack

[[ ${CATEGORY}/${PN} != dev-libs/rustlib ]] && DEPEND="|| (
	dev-libs/rustlib:=
	dev-util/cargo
)"

# @FUNCTION: coreos-cargo_src_unpack
# @DESCRIPTION:
# This amends the src_unpack from cargo.eclass to add support for Rust
# cross-compiling to the ChromeOS targets.  It maps the host triplet to
# one built into rustc and uses the board root as its sysroot.
coreos-cargo_src_unpack() {
	debug-print-function ${FUNCNAME} "$@"
	cargo_src_unpack "$@"

	[[ ${CBUILD:-${CHOST}} != ${CHOST} ]] || return 0

	# Map the SDK host triplet to one that is built into rustc.
	function rust_builtin_target() case "$1" in
		aarch64-*-linux-gnu) echo aarch64-unknown-linux-gnu ;;
		x86_64-*-linux-gnu) echo x86_64-unknown-linux-gnu ;;
		*) die "Unknown host triplet: $1" ;;
	esac

	# Set the gcc-rs flags for cross-compiling.
	export TARGET_CFLAGS="${CFLAGS}"
	export TARGET_CXXFLAGS="${CXXFLAGS}"

	# Wrap ar for gcc-rs to work around rust-lang/cargo#4456.
	export TARGET_AR="${T}/rustproof-ar"
	cat <<- EOF > "${TARGET_AR}" && chmod 0755 "${TARGET_AR}"
	#!/bin/sh
	unset LD_LIBRARY_PATH
	exec $(tc-getAR) "\$@"
	EOF

	# Wrap gcc for gcc-rs to work around rust-lang/cargo#4456.
	export TARGET_CC="${T}/rustproof-cc"
	cat <<- EOF > "${TARGET_CC}" && chmod 0755 "${TARGET_CC}"
	#!/bin/sh
	unset LD_LIBRARY_PATH
	exec $(tc-getCC) "\$@"
	EOF

	# Wrap g++ for gcc-rs to work around rust-lang/cargo#4456.
	export TARGET_CXX="${T}/rustproof-cxx"
	cat <<- EOF > "${TARGET_CXX}" && chmod 0755 "${TARGET_CXX}"
	#!/bin/sh
	unset LD_LIBRARY_PATH
	exec $(tc-getCXX) "\$@"
	EOF

	# Create a compiler wrapper that uses a sysroot for cross-compiling.
	export RUSTC_WRAPPER="${T}/wrustc"
	cat <<- 'EOF' > "${RUSTC_WRAPPER}" && chmod 0755 "${RUSTC_WRAPPER}"
	#!/bin/bash -e
	rustc=${1:?Missing rustc command}
	shift
	xflags=()
	[ "x$*" = "x${*#--target}" ] || xflags=( --sysroot="${ROOT:-/}usr" )
	exec "${rustc}" "${xflags[@]}" "$@"
	EOF

	# Compile for the built-in target, using the SDK cross-tools.
	export RUST_TARGET=$(rust_builtin_target "${CHOST}")
	cat <<- EOF >> "${ECARGO_HOME}/config"

	[build]
	target = "${RUST_TARGET}"

	[target.${RUST_TARGET}]
	ar = "${TARGET_AR}"
	linker = "${TARGET_CC}"
	EOF
}

fi
