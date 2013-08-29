# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="2"

# This project checks out the proto files from the read only repository
# linked to the src/chrome/browser/policy/proto directory of the Chromium
# project. It is not cros-work-able if changes to the protobufs are needed
# these should be done in the Chromium repository.

EGIT_REPO_SERVER="https://chromium.googlesource.com"
EGIT_REPO_URI="${EGIT_REPO_SERVER}/chromium/src/chrome/browser/policy/proto.git"
EGIT_PROJECT="proto"
EGIT_COMMIT="18f481b411ea0a861f0879af2065effce0e1fe6c"

inherit git

DESCRIPTION="Protobuf installer for the device policy proto definitions."
HOMEPAGE="http://chromium.org"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

DEPEND="!<=chromeos-base/chromeos-chrome-16.0.886.0_rc-r1
	!=chromeos-base/chromeos-chrome-16.0.882.0_alpha-r1"
RDEPEND="${DEPEND}"

src_prepare() {
	cp "${FILESDIR}"/policy_reader "${S}"
}

src_install() {
	insinto /usr/include/proto
	doins "${S}"/*.proto || die "Can not install protobuf files."
	insinto /usr/share/protofiles
	doins "${S}"/chrome_device_policy.proto
	doins "${S}"/device_management_backend.proto
	dobin "${S}"/policy_reader
}
