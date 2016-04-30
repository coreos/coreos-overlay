# Copyright 2014 CoreOS, Inc
# Distributed under the terms of the GNU General Public License v2

EAPI=5

inherit coreos-go git

DESCRIPTION="A small utility which generates Go code from any file. Useful for
embedding binary data in a Go program."
HOMEPAGE="https://github.com/jteeuwen/go-bindata"

COREOS_GO_PACKAGE="github.com/jteeuwen/go-bindata"
EGIT_REPO_URI="https://github.com/jteeuwen/go-bindata"
EGIT_COMMIT="4a8e91e5cd96381a2d96bfa7541e63a81f7a3784"

LICENSE="CC0 1.0 Universal"
SLOT="0"
KEYWORDS="amd64 arm64 ~x86 ppc64"

src_compile() {
	go_build "${COREOS_GO_PACKAGE}"/go-bindata
}
