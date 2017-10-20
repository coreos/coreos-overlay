# Copyright (c) 2017 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=6

CROS_WORKON_PROJECT="coreos/update-ssh-keys"
CROS_WORKON_LOCALNAME="update-ssh-keys"
CROS_WORKON_REPO="git://github.com"

if [[ ${PV} == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="fcbbf62deb7bbe1df527d71c8b6c0231cb1090cb" # v0.1.0
	KEYWORDS="amd64 arm64"
fi

inherit coreos-cargo cros-workon

DESCRIPTION="Utility for managing OpenSSH authorized public keys"
HOMEPAGE="https://github.com/coreos/update-ssh-keys"
LICENSE="Apache-2.0"
SLOT="0"

# sed -n 's/^"checksum \([^ ]*\) \([^ ]*\) .*/\1-\2/p' Cargo.lock
CRATES="
ansi_term-0.9.0
atty-0.2.3
base64-0.6.0
bitflags-0.9.1
block-buffer-0.2.0
byte-tools-0.2.0
byteorder-1.1.0
clap-2.26.2
digest-0.6.2
error-chain-0.11.0
fake-simd-0.1.2
fs2-0.4.2
generic-array-0.8.3
kernel32-sys-0.2.2
libc-0.2.31
nodrop-0.1.9
odds-0.2.25
openssh-keys-0.1.2
redox_syscall-0.1.31
redox_termios-0.1.1
safemem-0.2.0
sha2-0.6.0
strsim-0.6.0
term_size-0.3.0
termion-1.5.1
textwrap-0.8.0
typenum-1.9.0
unicode-width-0.1.4
users-0.6.0
vec_map-0.8.0
winapi-0.2.8
winapi-build-0.1.1
"

SRC_URI="$(cargo_crate_uris ${CRATES})"

src_unpack() {
	cros-workon_src_unpack "$@"
	coreos-cargo_src_unpack "$@"
}
