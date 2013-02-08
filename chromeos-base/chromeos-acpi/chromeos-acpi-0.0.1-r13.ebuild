# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_COMMIT="bc506a513266688ccfcafdff1567b7a049d39d7c"
CROS_WORKON_TREE="97011cdf8ff5934f680c71756dea9286dc73ec11"
CROS_WORKON_PROJECT="chromiumos/platform/acpi"

inherit cros-workon

DESCRIPTION="Chrome OS ACPI Scripts"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""
LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE=""

DEPEND=""

RDEPEND="sys-power/acpid
         chromeos-base/chromeos-init"

CROS_WORKON_LOCALNAME="acpi"

src_install() {
  dodir /etc/acpi/events
  dodir /etc/acpi

  install -m 0755 -o root -g root "${S}"/event_* "${D}"/etc/acpi/events
  install -m 0755 -o root -g root "${S}"/action_* "${D}"/etc/acpi

  dodir /etc/init
  install --owner=root --group=root --mode=0644 "${S}"/acpid.conf \
                                                "${D}/etc/init/"

}
