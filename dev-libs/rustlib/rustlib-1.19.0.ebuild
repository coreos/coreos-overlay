# Copyright 2017 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI=6

inherit coreos-cargo

DESCRIPTION="Standard libraries for Rust"
HOMEPAGE="http://www.rust-lang.org/"
LICENSE="|| ( MIT Apache-2.0 ) BSD-1 BSD-2 BSD-4 UoI-NCSA"

CRATES="
cmake-0.1.24
filetime-0.1.10
gcc-0.3.51
libc-0.2.26
"
SRC_URI="
https://static.rust-lang.org/dist/rustc-${PV}-src.tar.gz
$(cargo_crate_uris ${CRATES})
"
S="${WORKDIR}/rustc-${PV}-src"

RDEPEND="!dev-lang/rust"

SLOT="0"
KEYWORDS="amd64 arm64"

src_configure() {
	# get rid of the top level Cargo.toml or else cargo tries to use this one
	# too, and we don't want that since we aren't building the whole compiler
	rm src/Cargo.toml

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

	# Make cargo use the wrappers as well.
	sed -i \
		-e "s,^ar *=.*,ar = \"${TARGET_AR}\"," \
		-e "s,^linker *=.*,linker = \"${TARGET_CC}\"," \
		"${ECARGO_HOME}/config"
}

src_compile() {
	# put the target dir somewhere predictable
	local -x CARGO_TARGET_DIR="${T}/target"
	# look up required crates in our fake registry
	local -x CARGO_HOME="${ECARGO_HOME}"
	# fake out the compiler so that it lets us use #!feature attributes
	# really building the std libs is bootstrapping anyway, so I don't feel bad
	local -x RUSTC_BOOTSTRAP=1
	# various required flags for compiling thes std libs
	local -x RUSTFLAGS="-Z force-unstable-if-unmarked"
	# make sure we can find the custom ChromeOS target definitions
	local -x RUST_TARGET_PATH="/usr/$(get_libdir)/rustlib"

	# build the std lib, which also builds all the other important libraries
	# (core, collections, etc). build it for the target we want for this build.
	# also make sure that the jemalloc libraries are included.
	local -a features=( $(usex debug debug-jemalloc jemalloc) panic-unwind )
	cargo build -p std -v $(usex debug '' --release) \
		--features "${features[*]}" \
		--manifest-path src/libstd/Cargo.toml \
		--target "${RUST_TARGET}"

	# Correct the directory name prior to installing it.
	mv "${T}/target/${RUST_TARGET}/release/deps" "${T}/target/${RUST_TARGET}/release/lib"
}

src_install() {
	insinto "/usr/$(get_libdir)/rustlib/${RUST_TARGET}"
	doins -r "${T}/target/${RUST_TARGET}/release/lib"
}
