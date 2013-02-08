# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

# TODO(jsalz): Remove this ebuild; it's no longer used.

EAPI="4"
CROS_WORKON_COMMIT="c759366a1dd3d733b12bb2edc3bae9868d38ee5b"
CROS_WORKON_TREE="46e050754b5a2f5392223d734036b7b51dde5b5b"
CROS_WORKON_PROJECT="chromiumos/platform/factory-utils"

inherit cros-workon

DESCRIPTION="Factory development utilities for ChromiumOS"
HOMEPAGE="http://www.chromium.org/"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="cros_factory_bundle"

CROS_WORKON_LOCALNAME="factory-utils"
RDEPEND=""

# chromeos-installer for solving "lib/chromeos-common.sh" symlink.
# vboot_reference for binary programs (ex, cgpt).
DEPEND="chromeos-base/chromeos-installer[cros_host]
        chromeos-base/vboot_reference"

src_compile() {
    true
}

src_install() {
    true
}
