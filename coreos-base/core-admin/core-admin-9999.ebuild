#
# Copyright (c) 2013 Brandon Philips. All rights reserved.
# Distributed under the terms of the MIT License
# $Header:$
#

EAPI=2
CROS_WORKON_PROJECT="coreos/core-admin"
CROS_WORKON_LOCALNAME="core-admin"
inherit toolchain-funcs cros-workon

DESCRIPTION="systemd over rest"
HOMEPAGE="https://github.com/coreos/core-admin"
SRC_URI=""

LICENSE="MIT"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE=""

DEPEND=">=dev-lang/go-1.0.2"

src_compile() {
	export GOPATH="${S}"
	go get
	go build -o ${PN} || die
}

src_install() {
	dobin ${S}/${PN}
}
