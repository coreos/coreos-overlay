#
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Copyright (c) 2013 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=2
CROS_WORKON_PROJECT="xiangli-cmu/etcd-client"
CROS_WORKON_LOCALNAME="etcd-client"
CROS_WORKON_REPO="git://github.com"
inherit toolchain-funcs cros-workon systemd

DESCRIPTION="etcd-client"
HOMEPAGE="https://github.com/xiangli-cmu/etcd-client"
SRC_URI=""

LICENSE="MIT"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

DEPEND=">=dev-lang/go-1.0.2"
GOROOT="${ED}usr/$(get_libdir)/go"
GOPKG="${PN}"

src_compile() {
	export GOPATH="${S}"
	go install "${PN}"
}

src_install() {
	dobin ${S}/etcd-client
}

