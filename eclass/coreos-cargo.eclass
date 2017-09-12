# Copyright 2017 CoreOS, Inc.
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
	dev-libs/rustlib
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

	# Define the cross-compilers for rust-gcc commands.
	export TARGET_AR="$(tc-getAR)"
	export TARGET_CC="$(tc-getCC)"
	export TARGET_CFLAGS="${CFLAGS}"
	export TARGET_CXX="$(tc-getCXX)"
	export TARGET_CXXFLAGS="${CXXFLAGS}"

	# Map the SDK host triplet to one that is built into rustc.
	function rust_builtin_target() case "$1" in
		aarch64-*-linux-gnu) echo aarch64-unknown-linux-gnu ;;
		x86_64-*-linux-gnu) echo x86_64-unknown-linux-gnu ;;
		*) die "Unknown host triplet: $1" ;;
	esac

	# Allow searching host libraries when targeting incompatible systems.
	export RUST_TARGET=$(rust_builtin_target "${CHOST}")
	local build_triplet=$(rust_builtin_target "${CBUILD}")
	[[ ${RUST_TARGET} = ${build_triplet} ]] ||
	local crossflags="-L /usr/lib64/rustlib/${build_triplet}/lib"

	# Create a compiler wrapper to work around rust-lang/cargo#4456.
	if [[ ${CATEGORY}/${PN} != dev-libs/rustlib ]]; then
		cat <<- EOF > "${T}/wrustc" && chmod 0755 "${T}/wrustc"
		#!/bin/sh
		exec rustc --sysroot="${ROOT:-/}usr" ${crossflags-} "\$@"
		EOF
		export RUSTC="${T}/wrustc"
	fi

	# Compile for the built-in target, using the SDK cross-tools.
	cat <<- EOF >> "${ECARGO_HOME}/config"

	[build]
	target = "${RUST_TARGET}"

	[target.${RUST_TARGET}]
	ar = "${TARGET_AR}"
	linker = "${TARGET_CC}"
	EOF
}

fi
