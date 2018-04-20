# Copyright (c) 2017 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=6

CROS_WORKON_PROJECT="coreos/coreos-metadata"
CROS_WORKON_LOCALNAME="coreos-metadata"
CROS_WORKON_REPO="git://github.com"

UPDATE_SSH_KEYS_VERSION="0.1.2"

if [[ ${PV} == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="254093086f15d8380cc0e7f7e58e8e5d43b379a3" # v1.0.6
	KEYWORDS="amd64 arm64"
fi

inherit coreos-cargo cros-workon systemd

DESCRIPTION="A tool for collecting instance metadata from various providers"
HOMEPAGE="https://github.com/coreos/coreos-metadata"
LICENSE="Apache-2.0"
SLOT="0"

src_unpack() {
	cros-workon_src_unpack "$@"
	coreos-cargo_src_unpack "$@"
	unpack update-ssh-keys-${UPDATE_SSH_KEYS_VERSION}.tar.gz
}

src_prepare() {
	default

	# check to make sure we are using the right version of update-ssh-keys
	# our Cargo.toml should be using this version string as a tag specifier
	grep -q "^update-ssh-keys.*tag.*${UPDATE_SSH_KEYS_VERSION}" Cargo.toml || die "couldn't find update-ssh-keys version ${UPDATE_SSH_KEYS_VERSION} in Cargo.toml"

	sed -i "s;^update-ssh-keys.*;update-ssh-keys = { path = \"${WORKDIR}/update-ssh-keys-${UPDATE_SSH_KEYS_VERSION}\" };" Cargo.toml

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

# sed -n 's/^"checksum \([^ ]*\) \([^ ]*\) .*/\1-\2/p' Cargo.lock
CRATES="
adler32-1.0.2
aho-corasick-0.6.4
ansi_term-0.11.0
arrayref-0.3.4
arrayvec-0.4.7
atty-0.2.8
base64-0.6.0
base64-0.8.0
base64-0.9.0
bitflags-0.5.0
bitflags-0.7.0
bitflags-0.9.1
bitflags-1.0.1
block-buffer-0.3.3
build_const-0.2.1
byte-tools-0.2.0
byteorder-1.2.2
bytes-0.4.6
cc-1.0.10
cfg-if-0.1.2
chrono-0.4.2
clap-2.31.2
clippy-0.0.195
clippy_lints-0.0.195
core-foundation-0.2.3
core-foundation-sys-0.2.3
crc-1.7.0
crossbeam-0.2.12
crossbeam-deque-0.3.0
crossbeam-epoch-0.4.1
crossbeam-utils-0.2.2
crossbeam-utils-0.3.2
digest-0.7.2
dtoa-0.4.2
either-1.5.0
error-chain-0.11.0
fake-simd-0.1.2
foreign-types-0.3.2
foreign-types-shared-0.1.1
fs2-0.4.3
fuchsia-zircon-0.3.3
fuchsia-zircon-sys-0.3.3
futures-0.1.21
futures-cpupool-0.1.8
gcc-0.3.54
generic-array-0.9.0
getopts-0.2.17
glob-0.2.11
hostname-0.1.4
http-muncher-0.3.1
httparse-1.2.4
hyper-0.11.25
hyper-tls-0.1.3
idna-0.1.4
if_chain-0.1.2
iovec-0.1.2
ipnetwork-0.12.8
isatty-0.1.7
itertools-0.7.8
itoa-0.3.4
itoa-0.4.1
kernel32-sys-0.2.2
language-tags-0.2.2
lazy_static-0.2.11
lazy_static-1.0.0
lazycell-0.6.0
libc-0.2.40
libflate-0.1.14
log-0.3.9
log-0.4.1
matches-0.1.6
md-5-0.7.0
memchr-2.0.1
memoffset-0.2.1
mime-0.3.5
mio-0.6.14
miow-0.2.1
mockito-0.9.1
native-tls-0.1.5
net2-0.2.32
nix-0.9.0
nodrop-0.1.12
num-integer-0.1.36
num-traits-0.2.2
num_cpus-1.8.0
openssh-keys-0.2.2
openssl-0.9.24
openssl-sys-0.9.28
percent-encoding-1.0.1
pkg-config-0.3.9
pnet-0.21.0
pnet_base-0.21.0
pnet_datalink-0.21.0
pnet_macros-0.21.0
pnet_macros_support-0.21.0
pnet_packet-0.21.0
pnet_sys-0.21.0
pnet_transport-0.21.0
proc-macro2-0.3.6
pulldown-cmark-0.1.2
quine-mc_cluskey-0.2.4
quote-0.5.1
rand-0.3.22
rand-0.4.2
redox_syscall-0.1.37
redox_termios-0.1.1
regex-0.2.10
regex-syntax-0.5.5
relay-0.1.1
remove_dir_all-0.5.1
reqwest-0.7.3
rustc-serialize-0.3.24
safemem-0.2.0
schannel-0.1.12
scoped-tls-0.1.1
scopeguard-0.3.3
security-framework-0.1.16
security-framework-sys-0.1.16
semver-0.9.0
semver-parser-0.7.0
serde-1.0.41
serde-xml-rs-0.2.1
serde_derive-1.0.41
serde_derive_internals-0.23.1
serde_json-1.0.15
serde_urlencoded-0.5.1
sha2-0.7.0
slab-0.3.0
slab-0.4.0
slog-2.2.3
slog-async-2.3.0
slog-scope-4.0.1
slog-term-2.4.0
smallvec-0.2.1
strsim-0.7.0
syn-0.13.1
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
textwrap-0.9.0
thread_local-0.3.5
time-0.1.39
tokio-0.1.5
tokio-core-0.1.17
tokio-executor-0.1.2
tokio-io-0.1.6
tokio-proto-0.1.1
tokio-reactor-0.1.1
tokio-service-0.1.0
tokio-tcp-0.1.0
tokio-threadpool-0.1.2
tokio-timer-0.2.1
tokio-tls-0.1.4
tokio-udp-0.1.0
toml-0.4.6
typenum-1.10.0
ucd-util-0.1.1
unicase-2.1.0
unicode-bidi-0.3.4
unicode-normalization-0.1.5
unicode-width-0.1.4
unicode-xid-0.0.3
unicode-xid-0.1.0
unreachable-1.0.0
url-1.7.0
users-0.6.1
utf8-ranges-1.0.0
vcpkg-0.2.3
vec_map-0.8.0
version_check-0.1.3
void-1.0.2
winapi-0.2.8
winapi-0.3.4
winapi-build-0.1.1
winapi-i686-pc-windows-gnu-0.4.0
winapi-x86_64-pc-windows-gnu-0.4.0
winutil-0.1.1
ws2_32-sys-0.2.1
xml-rs-0.3.6
"
# not listed:
# update-ssh-keys-0.1.2

SRC_URI="$(cargo_crate_uris ${CRATES})
https://github.com/coreos/update-ssh-keys/archive/v${UPDATE_SSH_KEYS_VERSION}.tar.gz -> update-ssh-keys-${UPDATE_SSH_KEYS_VERSION}.tar.gz
"
