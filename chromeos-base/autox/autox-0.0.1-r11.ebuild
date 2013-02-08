# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_COMMIT="0e87afb2d4f6cef2b5ecca593e21117a5e9379a0"
CROS_WORKON_TREE="611b421824a85dff20fd67f6cb25b6eab9f36730"
CROS_WORKON_PROJECT="chromiumos/platform/autox"

inherit python cros-workon

DESCRIPTION="AutoX library for interacting with X apps"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

RDEPEND="dev-python/python-xlib"
DEPEND=

src_install() {
	insinto "$(python_get_sitedir)"
	doins autox.py || die
}
