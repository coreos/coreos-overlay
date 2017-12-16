# Copyright (c) 2017 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=6

CROS_WORKON_PROJECT="coreos/update-ssh-keys"
CROS_WORKON_LOCALNAME="update-ssh-keys"
CROS_WORKON_REPO="git://github.com"

if [[ ${PV} == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="380418faa84e5077358b49b067df45f95a9e867c" # v0.1.2
	KEYWORDS="amd64 arm64"
fi

inherit coreos-cargo cros-workon

DESCRIPTION="Utility for managing OpenSSH authorized public keys"
HOMEPAGE="https://github.com/coreos/update-ssh-keys"
LICENSE="Apache-2.0"
SLOT="0"

# make sure we have a new enough coreos-init that we won't conflict with the
# old bash script
RDEPEND="!<coreos-base/coreos-init-0.0.1-r152"

# sed -n 's/^"checksum \([^ ]*\) \([^ ]*\) .*/\1-\2/p' Cargo.lock
CRATES="
ansi_term-0.10.2
arrayref-0.3.4
atty-0.2.3
base64-0.8.0
bitflags-1.0.1
block-buffer-0.3.3
byte-tools-0.2.0
byteorder-1.2.1
clap-2.29.0
digest-0.7.2
error-chain-0.11.0
fake-simd-0.1.2
fs2-0.4.2
generic-array-0.9.0
kernel32-sys-0.2.2
libc-0.2.34
md-5-0.7.0
openssh-keys-0.2.2
redox_syscall-0.1.32
redox_termios-0.1.1
safemem-0.2.0
sha2-0.7.0
strsim-0.6.0
termion-1.5.1
textwrap-0.9.0
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
