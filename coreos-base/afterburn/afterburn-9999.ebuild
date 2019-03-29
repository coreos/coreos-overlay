# Copyright (c) 2017 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=7

CROS_WORKON_PROJECT="coreos/afterburn"
CROS_WORKON_LOCALNAME="afterburn"
CROS_WORKON_REPO="git://github.com"

if [[ ${PV} == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="d5de619f19e3f4ad20a9d2e5803ba0353618f00c" # v4.0.0
	KEYWORDS="amd64 arm64"
fi

inherit cargo cros-workon systemd

DESCRIPTION="A tool for collecting instance metadata from various providers"
HOMEPAGE="https://github.com/coreos/afterburn"
LICENSE="Apache-2.0"
SLOT="0"
RDEPEND="!coreos-base/coreos-metadata"

# sed -n 's/^"checksum \([^ ]*\) \([^ ]*\) .*/\1-\2/p' Cargo.lock
CRATES="
adler32-1.0.3
aho-corasick-0.6.9
ansi_term-0.11.0
arrayvec-0.4.9
atty-0.2.11
base64-0.10.1
base64-0.9.3
bitflags-0.5.0
bitflags-0.7.0
bitflags-1.0.4
block-buffer-0.7.0
block-padding-0.1.2
byte-tools-0.3.0
byteorder-1.3.1
bytes-0.4.11
cc-1.0.26
cfg-if-0.1.6
chrono-0.4.6
clap-2.32.0
cloudabi-0.0.3
colored-1.6.1
core-foundation-0.5.1
core-foundation-sys-0.5.1
crc32fast-1.1.2
crossbeam-0.2.12
crossbeam-deque-0.6.3
crossbeam-epoch-0.7.0
crossbeam-utils-0.6.3
difference-2.0.0
digest-0.8.0
dtoa-0.4.3
encoding_rs-0.8.13
error-chain-0.12.0
fake-simd-0.1.2
fnv-1.0.6
foreign-types-0.3.2
foreign-types-shared-0.1.1
fs2-0.4.3
fuchsia-zircon-0.3.3
fuchsia-zircon-sys-0.3.3
futures-0.1.25
futures-cpupool-0.1.8
generic-array-0.12.0
glob-0.2.11
h2-0.1.14
hostname-0.1.5
http-0.1.14
httparse-1.3.3
hyper-0.12.18
hyper-tls-0.3.1
idna-0.1.5
indexmap-1.0.2
iovec-0.1.2
ipnetwork-0.14.0
isatty-0.1.9
itoa-0.4.3
kernel32-sys-0.2.2
lazy_static-0.2.11
lazy_static-1.2.0
lazycell-1.2.1
libc-0.2.47
libflate-0.1.19
lock_api-0.1.5
log-0.3.9
log-0.4.6
matches-0.1.8
md-5-0.8.0
memchr-2.1.2
memoffset-0.2.1
mime-0.3.13
mime_guess-2.0.0-alpha.6
mio-0.6.16
mio-uds-0.6.7
miow-0.2.1
mockito-0.17.0
native-tls-0.2.2
net2-0.2.33
nix-0.13.0
nodrop-0.1.13
num-integer-0.1.39
num-traits-0.2.6
num_cpus-1.9.0
opaque-debug-0.2.1
openssh-keys-0.4.1
openssl-0.10.20
openssl-probe-0.1.2
openssl-sys-0.9.43
owning_ref-0.4.0
parking_lot-0.6.4
parking_lot_core-0.3.1
percent-encoding-1.0.1
phf-0.7.23
phf_codegen-0.7.23
phf_generator-0.7.23
phf_shared-0.7.23
pkg-config-0.3.14
pnet-0.22.0
pnet_base-0.22.0
pnet_datalink-0.22.0
pnet_macros-0.21.0
pnet_macros_support-0.22.0
pnet_packet-0.22.0
pnet_sys-0.22.0
pnet_transport-0.22.0
proc-macro2-0.4.24
quote-0.6.10
rand-0.4.3
rand-0.5.5
rand-0.6.1
rand_chacha-0.1.0
rand_core-0.2.2
rand_core-0.3.0
rand_hc-0.1.0
rand_isaac-0.1.1
rand_pcg-0.1.1
rand_xorshift-0.1.0
redox_syscall-0.1.44
redox_termios-0.1.1
regex-0.2.11
regex-1.1.0
regex-syntax-0.5.6
regex-syntax-0.6.4
remove_dir_all-0.5.1
reqwest-0.9.5
rustc-serialize-0.3.24
rustc_version-0.2.3
ryu-0.2.7
safemem-0.3.0
schannel-0.1.14
scopeguard-0.3.3
security-framework-0.2.1
security-framework-sys-0.2.1
semver-0.9.0
semver-parser-0.7.0
serde-1.0.89
serde-xml-rs-0.2.1
serde_derive-1.0.89
serde_json-1.0.39
serde_urlencoded-0.5.4
sha2-0.8.0
siphasher-0.2.3
slab-0.4.1
slog-2.4.1
slog-async-2.3.0
slog-scope-4.0.1
slog-term-2.4.0
smallvec-0.6.7
stable_deref_trait-1.1.1
string-0.1.2
strsim-0.7.0
syn-0.15.23
syntex-0.42.2
syntex_errors-0.42.0
syntex_pos-0.42.0
syntex_syntax-0.42.0
take_mut-0.2.2
tempdir-0.3.7
tempfile-3.0.7
term-0.4.6
term-0.5.1
termion-1.5.1
textwrap-0.10.0
thread_local-0.3.6
time-0.1.41
tokio-0.1.13
tokio-codec-0.1.1
tokio-current-thread-0.1.4
tokio-executor-0.1.5
tokio-fs-0.1.4
tokio-io-0.1.10
tokio-reactor-0.1.7
tokio-tcp-0.1.2
tokio-threadpool-0.1.9
tokio-timer-0.2.8
tokio-udp-0.1.3
tokio-uds-0.2.4
try-lock-0.2.2
typenum-1.10.0
ucd-util-0.1.3
unicase-1.4.2
unicase-2.2.0
unicode-bidi-0.3.4
unicode-normalization-0.1.7
unicode-width-0.1.5
unicode-xid-0.0.3
unicode-xid-0.1.0
unreachable-1.0.0
update-ssh-keys-0.4.1
url-1.7.2
users-0.8.1
utf8-ranges-1.0.2
uuid-0.7.1
vcpkg-0.2.6
vec_map-0.8.1
version_check-0.1.5
void-1.0.2
want-0.0.6
winapi-0.2.8
winapi-0.3.6
winapi-build-0.1.1
winapi-i686-pc-windows-gnu-0.4.0
winapi-x86_64-pc-windows-gnu-0.4.0
winutil-0.1.1
ws2_32-sys-0.2.1
xml-rs-0.3.6
"

SRC_URI="$(cargo_crate_uris ${CRATES})"

src_unpack() {
	cros-workon_src_unpack "$@"
	cargo_src_unpack "$@"
}

src_prepare() {
	default

	# tell the rust-openssl bindings where the openssl library and include dirs are
	export PKG_CONFIG_ALLOW_CROSS=1
	export OPENSSL_LIB_DIR=/usr/lib64/
	export OPENSSL_INCLUDE_DIR=/usr/include/openssl/
}

src_compile() {
	cargo_src_compile --features cl-legacy "$@"
}

src_install() {
	cargo_src_install --features cl-legacy "$@"
	mv "${D}/usr/bin/afterburn" "${D}/usr/bin/coreos-metadata"

	systemd_dounit "${FILESDIR}/coreos-metadata.service"
	systemd_dounit "${FILESDIR}/coreos-metadata-sshkeys@.service"
}

src_test() {
	cargo_src_test --features cl-legacy "$@"
}
