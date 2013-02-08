# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"

DESCRIPTION="Adds packages that are required for testing."
HOMEPAGE="http://src.chromium.org"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="cros_ec"

# Packages required to support autotest images.  Dependencies here
# are for packages that must be present on a local device and that
# are not downloaded by the autotest server.  This includes both
# packages relied on by the server, as well as packages relied on by
# specific tests.
#
# This package is not meant to capture tools useful for test debug;
# use the chromeos-dev package for that purpose.
#
# Note that some packages used by autotest are actually built by the
# autotest package and downloaded by the server, regardless of
# whether the package is present on the target device; those
# packages aren't listed here.
#
# Developers should be aware that packages installed by this ebuild
# are rooted in /usr/local.  This means that libraries are installed
# in /usr/local/lib, executables in /usr/local/bin, etc.
#
# TODO(jrbarnette):  It's not known definitively that the list
# below contains no unneeded dependencies.  More work is needed to
# determine for sure that every package listed is actually used.
# TODO(core): 	chromeos-base/autox
#	chromeos-base/chromeos-test-init
#	chromeos-base/flimflam-test
#	chromeos-base/minifakedns
#	chromeos-base/modem-diagnostics
#	chromeos-base/protofiles
#	chromeos-base/recover-duts
#	chromeos-base/saft
#	chromeos-base/system_api
#	chromeos-base/touchbot
#	app-misc/tmux

RDEPEND="${RDEPEND}
	app-admin/sudo
	app-arch/gzip
	app-arch/tar
	app-crypt/tpm-tools
	dev-lang/python
	dev-python/dbus-python
	dev-python/pygobject
	dev-python/pyserial
	dev-python/pyudev
	dev-python/pyyaml
	dev-util/dbus-spy
	net-analyzer/netperf
	net-dns/dnsmasq
	net-misc/dhcp
	net-misc/iperf
	net-misc/iputils
	net-misc/openssh
	net-misc/rsync
	sys-apps/coreutils
	sys-apps/file
	sys-apps/findutils
	sys-apps/kbd
	sys-apps/memtester
	!arm? ( sys-apps/pciutils )
	x86? ( sys-apps/superiotool )
	sys-apps/shadow
	sys-devel/binutils
	sys-process/procps
	sys-process/psmisc
	sys-process/time
	cros_ec? ( chromeos-base/ec-utils )
	"
