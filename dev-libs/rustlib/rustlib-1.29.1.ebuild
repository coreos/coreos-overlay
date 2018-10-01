# Copyright 2017-2018 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI=6

inherit coreos-cargo

DESCRIPTION="Standard libraries for Rust"
HOMEPAGE="http://www.rust-lang.org/"
LICENSE="|| ( MIT Apache-2.0 ) BSD-1 BSD-2 BSD-4 UoI-NCSA"

SRC_URI="https://static.rust-lang.org/dist/rustc-${PV}-src.tar.gz"
S="${WORKDIR}/rustc-${PV}-src"

RDEPEND="!dev-lang/rust"

SLOT="0/${PVR}"
KEYWORDS="amd64 arm64"

src_configure() {
	# get rid of the top level Cargo.toml or else cargo tries to use this one
	# too, and we don't want that since we aren't building the whole compiler
	rm src/Cargo.toml

	# Add the vendored crates to the local registry.
	ln -fst "${ECARGO_VENDOR}" "${S}"/src/vendor/*
}

src_compile() {
	# put the target dir somewhere predictable
	local -x CARGO_TARGET_DIR="${T}/target"
	# look up required crates in our fake registry
	local -x CARGO_HOME="${ECARGO_HOME}"
	# set this required bootstrapping variable
	local -x CFG_COMPILER_HOST_TRIPLE="${RUST_TARGET}"
	# fake out the compiler so that it lets us use #!feature attributes
	# really building the std libs is bootstrapping anyway, so I don't feel bad
	local -x RUSTC_BOOTSTRAP=1
	# various required flags for compiling thes std libs
	local -x RUSTFLAGS="-C prefer-dynamic -L ${CARGO_TARGET_DIR}/${RUST_TARGET}/release/deps -Z force-unstable-if-unmarked"
	# set the metadata directory for the boostrap build
	local -x RUSTC_ERROR_METADATA_DST="${T}/target/release/build/tmp/extended-error-metadata"

	# build the std lib, which also builds all the other important libraries
	# (core, collections, etc). build it for the target we want for this build.
	# also make sure that the jemalloc libraries are included.
	local -a features=( $(usex debug debug-jemalloc jemalloc) panic-unwind )
	cargo build -p std -v --lib $(usex debug '' --release) \
		--features "${features[*]}" \
		--manifest-path src/libstd/Cargo.toml \
		--target "${RUST_TARGET}" || die

	cargo build -p term -v --lib $(usex debug '' --release) \
		--manifest-path src/libterm/Cargo.toml \
		--target "${RUST_TARGET}" || die

	# Correct the directory name prior to installing it.
	mv "${CARGO_TARGET_DIR}/${RUST_TARGET}/release/deps" "${CARGO_TARGET_DIR}/${RUST_TARGET}/release/lib"
}

src_install() {
	insinto "/usr/$(get_libdir)/rustlib/${RUST_TARGET}"
	doins -r "${T}/target/${RUST_TARGET}/release/lib"
}
