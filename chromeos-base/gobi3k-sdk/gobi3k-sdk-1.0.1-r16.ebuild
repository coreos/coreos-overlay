# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"

CROS_WORKON_COMMIT="b69d8f1d905b7db5296b5ff76d731ef88c79ef55"
CROS_WORKON_TREE="9ebc2b4fd3dfd75430562a5238a2df9218966fdc"
CROS_WORKON_PROJECT="chromiumos/third_party/gobi3k-sdk"
CROS_WORKON_LOCALNAME=../third_party/gobi3k-sdk
inherit cros-workon toolchain-funcs

DESCRIPTION="SDK for Qualcomm Gobi 3000 modems"

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

# TODO(jglasgow): remove realpath dependency
RDEPEND="
	|| ( >=sys-apps/coreutils-8.15 app-misc/realpath )
"

src_configure() {
	tc-export LD CXX CC OBJCOPY AR
}
