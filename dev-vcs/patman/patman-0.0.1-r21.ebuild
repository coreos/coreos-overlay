# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

EAPI=4
CROS_WORKON_COMMIT="f561cd93f05c290bd0280f467c4a10ff224f7bf9"
CROS_WORKON_TREE="8595e217edc9baad42dd848f58b4cbd0c85c0200"
PYTHON_DEPEND="2"
CROS_WORKON_PROJECT="chromiumos/third_party/u-boot"
CROS_WORKON_LOCALNAME="u-boot"
CROS_WORKON_SUBDIR="files/tools/patman"

inherit cros-workon distutils

DESCRIPTION="Patman tool (from U-Boot) for sending patches upstream"
HOMEPAGE="http://www.denx.de/wiki/U-Boot"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE=""

DEPEND="dev-python/setuptools"
RDEPEND=""

src_prepare() {
	rm patman
	cp "${FILESDIR}/setup.py" .
	touch __init__.py

	distutils_src_prepare
}

src_install() {
	dobin "${FILESDIR}/patman"

	distutils_src_install
}