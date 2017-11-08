# Copyright (c) 2017 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=6

CROS_WORKON_PROJECT="coreos/coreos-metadata"
CROS_WORKON_LOCALNAME="coreos-metadata"
CROS_WORKON_REPO="git://github.com"

UPDATE_SSH_KEYS_VERSION="0.1.0"

if [[ ${PV} == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="0fc18cbd66d2ed8a6a016861057138fd4043a79e" # v1.0.1
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
advapi32-sys-0.1.2
advapi32-sys-0.2.0
aho-corasick-0.6.3
ansi_term-0.9.0
atty-0.2.3
base64-0.6.0
bitflags-0.5.0
bitflags-0.7.0
bitflags-0.9.1
block-buffer-0.2.0
build_const-0.2.0
byte-tools-0.2.0
byteorder-1.1.0
bytes-0.4.5
cargo_metadata-0.2.3
cc-1.0.1
cfg-if-0.1.2
chrono-0.4.0
clap-2.26.2
clippy-0.0.165
clippy_lints-0.0.165
conv-0.3.3
core-foundation-0.2.3
core-foundation-sys-0.2.3
crc-1.5.0
crc-core-0.1.0
crossbeam-0.2.10
crypt32-sys-0.2.0
custom_derive-0.1.7
digest-0.6.2
dtoa-0.4.2
either-1.3.0
error-chain-0.11.0
fake-simd-0.1.2
foreign-types-0.2.0
fs2-0.4.2
fuchsia-zircon-0.2.1
fuchsia-zircon-sys-0.2.0
futures-0.1.16
futures-cpupool-0.1.7
gcc-0.3.54
generic-array-0.8.3
getopts-0.2.15
glob-0.2.11
hostname-0.1.3
http-muncher-0.3.1
httparse-1.2.3
hyper-0.11.6
hyper-tls-0.1.2
idna-0.1.4
iovec-0.1.1
ipnetwork-0.12.7
isatty-0.1.5
itertools-0.6.5
itoa-0.3.4
kernel32-sys-0.1.4
kernel32-sys-0.2.2
language-tags-0.2.2
lazy_static-0.2.9
lazycell-0.5.1
libc-0.2.32
libflate-0.1.12
log-0.3.8
magenta-0.1.1
magenta-sys-0.1.1
matches-0.1.6
memchr-1.0.1
mime-0.3.5
mio-0.6.10
miow-0.2.1
mockito-0.9.0
native-tls-0.1.4
net2-0.2.31
nix-0.9.0
nodrop-0.1.10
num-0.1.40
num-integer-0.1.35
num-iter-0.1.34
num-traits-0.1.40
num_cpus-1.7.0
odds-0.2.25
openssh-keys-0.1.2
openssl-0.9.20
openssl-sys-0.9.20
percent-encoding-1.0.0
pkg-config-0.3.9
pnet-0.19.0
pnet_macros-0.15.0
pnet_macros_support-0.2.0
pulldown-cmark-0.0.15
quine-mc_cluskey-0.2.4
quote-0.3.15
rand-0.3.17
redox_syscall-0.1.31
redox_termios-0.1.1
regex-0.2.2
regex-syntax-0.4.1
relay-0.1.0
reqwest-0.7.3
rustc-serialize-0.3.24
rustc_version-0.1.7
safemem-0.2.0
schannel-0.1.8
scoped-tls-0.1.0
secur32-sys-0.2.0
security-framework-0.1.16
security-framework-sys-0.1.16
semver-0.1.20
semver-0.6.0
semver-parser-0.7.0
serde-1.0.15
serde-xml-rs-0.2.1
serde_derive-1.0.15
serde_derive_internals-0.16.0
serde_json-1.0.4
serde_urlencoded-0.5.1
sha2-0.6.0
slab-0.3.0
slab-0.4.0
slog-2.0.12
slog-async-2.1.0
slog-scope-4.0.0
slog-term-2.2.0
smallvec-0.2.1
strsim-0.6.0
syn-0.11.11
synom-0.11.3
syntex-0.42.2
syntex_errors-0.42.0
syntex_pos-0.42.0
syntex_syntax-0.42.0
take-0.1.0
take_mut-0.1.3
tempdir-0.3.5
term-0.4.6
term_size-0.3.0
termion-1.5.1
textwrap-0.8.0
thread_local-0.3.4
time-0.1.38
tokio-core-0.1.10
tokio-io-0.1.3
tokio-proto-0.1.1
tokio-service-0.1.0
tokio-tls-0.1.3
toml-0.4.5
typenum-1.9.0
unicase-2.0.0
unicode-bidi-0.3.4
unicode-normalization-0.1.5
unicode-width-0.1.4
unicode-xid-0.0.3
unicode-xid-0.0.4
unreachable-1.0.0
url-1.5.1
users-0.6.0
utf8-ranges-1.0.0
vcpkg-0.2.2
vec_map-0.8.0
void-1.0.2
winapi-0.2.8
winapi-build-0.1.1
winutil-0.1.0
ws2_32-sys-0.2.1
xml-rs-0.3.6
"
# not listed:
# update-ssh-keys-0.1.0

SRC_URI="$(cargo_crate_uris ${CRATES})
https://github.com/coreos/update-ssh-keys/archive/v${UPDATE_SSH_KEYS_VERSION}.tar.gz -> update-ssh-keys-${UPDATE_SSH_KEYS_VERSION}.tar.gz
"
