# Copyright (c) 2018 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=6

CROS_WORKON_PROJECT="coreos/fero"
CROS_WORKON_LOCALNAME="fero"
CROS_WORKON_REPO="git://github.com"

if [[ ${PV} == 9999 ]]; then
	KEYWORDS="~amd64"
else
	CROS_WORKON_COMMIT="c6784bebaecaac4b1712fe17263890143e5c37c7" # v0.1.0
	KEYWORDS="amd64"
fi

inherit coreos-cargo cros-workon

DESCRIPTION="Client for fero signing server"
HOMEPAGE="https://github.com/coreos/fero"
LICENSE="LGPL-2.1+"
SLOT="0"

IUSE=""

DEPEND=">=dev-libs/protobuf-3.0.0"
RDEPEND="${DEPEND}"

src_unpack() {
	cros-workon_src_unpack "$@"
	coreos-cargo_src_unpack "$@"
}

src_compile() {
	export CARGO_HOME="${ECARGO_HOME}"
	cargo build -v -j $(makeopts_jobs) $(usex debug "" --release) -p ${PN} \
		|| die "cargo build failed"
}

src_install() {
	cargo install -j $(makeopts_jobs) --root="${D}/usr" $(usex debug --debug "") --path fero-client \
		|| die "cargo install failed"
	rm -f "${D}/usr/.crates.toml"
}

CRATES="
aho-corasick-0.6.4
ansi_term-0.11.0
arrayref-0.3.4
atty-0.2.10
backtrace-0.3.6
backtrace-sys-0.1.16
base64-0.8.0
bindgen-0.32.3
bit-vec-0.4.4
bitflags-1.0.3
block-buffer-0.3.3
byte-tools-0.2.0
byteorder-1.2.2
bzip2-0.3.2
bzip2-sys-0.1.6
cc-1.0.15
cexpr-0.2.3
cfg-if-0.1.2
chrono-0.4.2
clang-sys-0.21.2
clap-2.31.2
cmake-0.1.30
conv-0.3.3
cstr-argument-0.0.2
custom_derive-0.1.7
diesel-1.2.2
diesel-derive-enum-0.4.3
diesel_derives-1.2.0
diesel_migrations-1.2.0
digest-0.7.2
env_logger-0.4.3
error-chain-0.11.0
failure-0.1.1
failure_derive-0.1.1
fake-simd-0.1.2
flate2-1.0.1
fuchsia-zircon-0.3.3
fuchsia-zircon-sys-0.3.3
futures-0.1.21
gag-0.1.10
generic-array-0.9.0
glob-0.2.11
gpg-error-0.3.2
gpgme-0.7.2
gpgme-sys-0.7.0
grpcio-0.2.3
grpcio-compiler-0.2.0
grpcio-sys-0.2.3
heck-0.3.0
kernel32-sys-0.2.2
lazy_static-1.0.0
libc-0.2.40
libgpg-error-sys-0.3.2
libloading-0.4.3
libsqlite3-sys-0.9.1
libyubihsm-0.2.1
log-0.3.9
log-0.4.1
loggerv-0.7.1
md-5-0.7.0
memchr-1.0.2
memchr-2.0.1
migrations_internals-1.2.0
migrations_macros-1.2.0
miniz-sys-0.1.10
nom-3.2.1
num-0.1.42
num-bigint-0.1.43
num-complex-0.1.43
num-integer-0.1.36
num-iter-0.1.35
num-rational-0.1.42
num-traits-0.2.2
peeking_take_while-0.1.2
pem-0.5.0
pkg-config-0.3.11
pretty-good-0.2.2
proc-macro2-0.2.3
proc-macro2-0.4.6
protobuf-1.5.1
protoc-1.5.1
quote-0.3.15
quote-0.4.2
quote-0.6.3
rand-0.4.2
redox_syscall-0.1.37
redox_termios-0.1.1
regex-0.2.11
regex-syntax-0.5.6
remove_dir_all-0.5.1
ripemd160-0.7.0
rpassword-2.0.0
rustc-demangle-0.1.7
rustc-serialize-0.3.24
safemem-0.2.0
secstr-0.3.0
semver-0.9.0
semver-parser-0.7.0
sha-1-0.7.0
sha2-0.7.1
smallvec-0.6.1
strsim-0.7.0
structopt-0.2.10
structopt-derive-0.2.10
syn-0.11.11
syn-0.12.15
syn-0.14.4
synom-0.11.3
synstructure-0.6.1
tempdir-0.3.7
tempfile-3.0.2
termion-1.5.1
textwrap-0.9.0
thread_local-0.3.5
time-0.1.39
typenum-1.10.0
ucd-util-0.1.1
unicode-segmentation-1.2.1
unicode-width-0.1.4
unicode-xid-0.0.4
unicode-xid-0.1.0
unreachable-1.0.0
utf8-ranges-1.0.0
vcpkg-0.2.3
vec_map-0.8.0
void-1.0.2
which-1.0.5
winapi-0.2.8
winapi-0.3.4
winapi-build-0.1.1
winapi-i686-pc-windows-gnu-0.4.0
winapi-x86_64-pc-windows-gnu-0.4.0
yasna-0.1.3
"
SRC_URI="$(cargo_crate_uris ${CRATES})"
