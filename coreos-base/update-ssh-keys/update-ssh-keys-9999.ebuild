# Copyright (c) 2017 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=6

CROS_WORKON_PROJECT="coreos/update-ssh-keys"
CROS_WORKON_LOCALNAME="update-ssh-keys"
CROS_WORKON_REPO="git://github.com"

if [[ ${PV} == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="79449498bdfbbf62e6290dc76732bda619eed37e" # v0.3.0
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
ansi_term-0.11.0
arrayref-0.3.5
atty-0.2.11
base64-0.9.2
bitflags-1.0.3
block-buffer-0.3.3
byte-tools-0.2.0
byteorder-1.2.4
clap-2.32.0
digest-0.7.5
error-chain-0.12.0
fake-simd-0.1.2
fs2-0.4.3
generic-array-0.9.0
libc-0.2.43
md-5-0.7.0
openssh-keys-0.3.0
redox_syscall-0.1.40
redox_termios-0.1.1
safemem-0.2.0
sha2-0.7.1
strsim-0.7.0
termion-1.5.1
textwrap-0.10.0
typenum-1.10.0
unicode-width-0.1.5
users-0.7.0
vec_map-0.8.1
winapi-0.3.5
winapi-i686-pc-windows-gnu-0.4.0
winapi-x86_64-pc-windows-gnu-0.4.0
"

SRC_URI="$(cargo_crate_uris ${CRATES})"

src_unpack() {
	cros-workon_src_unpack "$@"
	coreos-cargo_src_unpack "$@"
}
