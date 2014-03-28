# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"

DESCRIPTION="Adds some developer niceties on top of Chrome OS for debugging"
HOMEPAGE="http://src.chromium.org"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="bluetooth opengl X"

# The dependencies here are meant to capture "all the packages
# developers want to use for development, test, or debug".  This
# category is meant to include all developer use cases, including
# software test and debug, performance tuning, hardware validation,
# and debugging failures running autotest.
#
# To protect developer images from changes in other ebuilds you
# should include any package with a user constituency, regardless of
# whether that package is included in the base Chromium OS image or
# any other ebuild.
#
# Don't include packages that are indirect dependencies: only
# include a package if a file *in that package* is expected to be
# useful.
# TODO: 	chromeos-base/chromeos-dev-init
#	chromeos-base/flimflam-test
#	chromeos-base/protofiles
#	chromeos-base/system_api
#	dev-util/hdctools
#	app-benchmarks/punybench
#	dev-util/libc-bench
RDEPEND="
	app-admin/sudo
	app-arch/gzip
	app-arch/tar
	dev-libs/nss
	app-editors/vim
	app-misc/evtest
	app-portage/gentoolkit
	app-shells/bash
	coreos-base/coreos
	coreos-base/gmerge
	dev-lang/python
	dev-python/dbus-python
	dev-python/pygobject
	dev-util/strace
	net-analyzer/netperf
	net-analyzer/tcpdump
	net-dialup/minicom
	net-misc/dhcp
	net-misc/iperf
	net-misc/iputils
	net-misc/openssh
	net-misc/rsync
	sys-apps/coreutils
	sys-apps/diffutils
	sys-apps/file
	sys-apps/findutils
	sys-apps/i2c-tools
	sys-apps/kbd
	sys-apps/less
	sys-apps/portage
	sys-apps/smartmontools
	sys-apps/usbutils
	sys-apps/which
	sys-devel/gcc
	sys-devel/gdb
	sys-fs/fuse
	sys-fs/lvm2
	sys-fs/sshfs-fuse
	sys-process/ktop
	sys-process/procps
	sys-process/psmisc
	sys-process/time
	virtual/python-argparse
	coreos-base/coreos-experimental
	"

# TODO:  sys-apps/iotools
X86_DEPEND="
	app-benchmarks/i7z
	app-editors/qemacs
	sys-apps/dmidecode
	sys-apps/pciutils
"

RDEPEND="${RDEPEND} x86? ( ${X86_DEPEND} )"
RDEPEND="${RDEPEND} amd64? ( ${X86_DEPEND} )"
