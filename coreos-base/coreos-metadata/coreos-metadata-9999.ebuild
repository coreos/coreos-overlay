# Copyright (c) 2017 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=6

CROS_WORKON_PROJECT="coreos/coreos-metadata"
CROS_WORKON_LOCALNAME="coreos-metadata"
CROS_WORKON_REPO="git://github.com"

if [[ ${PV} == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="ea8be56a085f3f845aab56b6d167fce4855bc4a3" # v3.0.0
	KEYWORDS="amd64 arm64"
fi

inherit coreos-cargo cros-workon systemd

DESCRIPTION="A tool for collecting instance metadata from various providers"
HOMEPAGE="https://github.com/coreos/coreos-metadata"
LICENSE="Apache-2.0"
SLOT="0"

# sed -n 's/^"checksum \([^ ]*\) \([^ ]*\) .*/\1-\2/p' Cargo.lock
CRATES="
adler32-1.0.3
aho-corasick-0.6.6
ansi_term-0.11.0
arrayref-0.3.5
arrayvec-0.4.7
atty-0.2.11
base64-0.9.2
bitflags-0.5.0
bitflags-0.7.0
bitflags-0.9.1
bitflags-1.0.3
block-buffer-0.3.3
build_const-0.2.1
byte-tools-0.2.0
byteorder-1.2.4
bytes-0.4.9
cc-1.0.18
cfg-if-0.1.5
chrono-0.4.5
clap-2.32.0
core-foundation-0.2.3
core-foundation-sys-0.2.3
crc-1.8.1
crossbeam-0.2.12
crossbeam-deque-0.3.1
crossbeam-epoch-0.4.3
crossbeam-utils-0.3.2
digest-0.7.5
dtoa-0.4.3
error-chain-0.12.0
fake-simd-0.1.2
foreign-types-0.3.2
foreign-types-shared-0.1.1
fs2-0.4.3
fuchsia-zircon-0.3.3
fuchsia-zircon-sys-0.3.3
futures-0.1.23
futures-cpupool-0.1.8
generic-array-0.9.0
glob-0.2.11
hostname-0.1.5
http-muncher-0.3.2
httparse-1.3.2
hyper-0.11.27
hyper-tls-0.1.4
idna-0.1.5
iovec-0.1.2
ipnetwork-0.12.8
isatty-0.1.8
itoa-0.4.2
kernel32-sys-0.2.2
language-tags-0.2.2
lazy_static-0.2.11
lazy_static-1.1.0
lazycell-0.6.0
libc-0.2.43
libflate-0.1.16
log-0.3.9
log-0.4.3
matches-0.1.7
md-5-0.7.0
memchr-2.0.1
memoffset-0.2.1
mime-0.3.9
mio-0.6.15
miow-0.2.1
mockito-0.9.1
native-tls-0.1.5
net2-0.2.33
nix-0.9.0
nodrop-0.1.12
num-integer-0.1.39
num-traits-0.2.5
num_cpus-1.8.0
openssh-keys-0.3.0
openssl-0.9.24
openssl-sys-0.9.35
percent-encoding-1.0.1
pkg-config-0.3.13
pnet-0.21.0
pnet_base-0.21.0
pnet_datalink-0.21.0
pnet_macros-0.21.0
pnet_macros_support-0.21.0
pnet_packet-0.21.0
pnet_sys-0.21.0
pnet_transport-0.21.0
proc-macro2-0.4.13
quote-0.6.6
rand-0.3.22
rand-0.4.3
redox_syscall-0.1.40
redox_termios-0.1.1
regex-0.2.11
regex-syntax-0.5.6
relay-0.1.1
remove_dir_all-0.5.1
reqwest-0.7.3
rustc-serialize-0.3.24
ryu-0.2.3
safemem-0.2.0
schannel-0.1.13
scoped-tls-0.1.2
scopeguard-0.3.3
security-framework-0.1.16
security-framework-sys-0.1.16
serde-1.0.71
serde-xml-rs-0.2.1
serde_derive-1.0.71
serde_json-1.0.26
serde_urlencoded-0.5.3
sha2-0.7.1
slab-0.3.0
slab-0.4.1
slog-2.3.2
slog-async-2.3.0
slog-scope-4.0.1
slog-term-2.4.0
smallvec-0.2.1
strsim-0.7.0
syn-0.14.8
syntex-0.42.2
syntex_errors-0.42.0
syntex_pos-0.42.0
syntex_syntax-0.42.0
take-0.1.0
take_mut-0.2.2
tempdir-0.3.7
term-0.4.6
term-0.5.1
termion-1.5.1
textwrap-0.10.0
thread_local-0.3.6
time-0.1.40
tokio-0.1.7
tokio-codec-0.1.0
tokio-core-0.1.17
tokio-executor-0.1.3
tokio-fs-0.1.3
tokio-io-0.1.7
tokio-proto-0.1.1
tokio-reactor-0.1.3
tokio-service-0.1.0
tokio-tcp-0.1.1
tokio-threadpool-0.1.5
tokio-timer-0.2.5
tokio-tls-0.1.4
tokio-udp-0.1.1
try-lock-0.1.0
typenum-1.10.0
ucd-util-0.1.1
unicase-2.1.0
unicode-bidi-0.3.4
unicode-normalization-0.1.7
unicode-width-0.1.5
unicode-xid-0.0.3
unicode-xid-0.1.0
update-ssh-keys-0.3.0
url-1.7.1
users-0.7.0
utf8-ranges-1.0.0
vcpkg-0.2.5
vec_map-0.8.1
version_check-0.1.4
void-1.0.2
want-0.0.4
winapi-0.2.8
winapi-0.3.5
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
	coreos-cargo_src_unpack "$@"
}

src_prepare() {
	default

	# tell the rust-openssl bindings where the openssl library and include dirs are
	export PKG_CONFIG_ALLOW_CROSS=1
	export OPENSSL_LIB_DIR=/usr/lib64/
	export OPENSSL_INCLUDE_DIR=/usr/include/openssl/
}

src_install() {
	cargo_src_install

	systemd_dounit "${FILESDIR}/coreos-metadata.service"
	systemd_dounit "${FILESDIR}/coreos-metadata-sshkeys@.service"
}
