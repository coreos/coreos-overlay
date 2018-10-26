# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2

DESCRIPTION="List of packages that are needed on the buildhost (meta package)"
HOMEPAGE="http://src.chromium.org"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE=""

# Needed to run setup crossdev, run build scripts, and make a bootable image.
RDEPEND="${RDEPEND}
	app-arch/lzop
	app-arch/pigz
	app-admin/sudo
	sys-apps/less
	dev-embedded/u-boot-tools
	dev-util/ccache
	dev-util/crosutils
	sys-boot/syslinux
	sys-devel/crossdev
	sys-devel/sysroot-wrappers
	sys-fs/dosfstools
	"

# Host dependencies for building cross-compiled packages.
# TODO: chromeos-base/chromeos-installer
RDEPEND="${RDEPEND}
	>=app-arch/pbzip2-1.1.1-r1
	app-arch/rpm2targz
	app-arch/sharutils
	app-arch/unzip
	app-emulation/qemu
	coreos-base/cros-devutils[cros_host]
	=dev-lang/python-2*
	dev-python/setuptools
	dev-lang/nasm
	dev-lang/swig
	dev-lang/yasm
	dev-lang/go:1.6
	dev-lang/go:1.7
	dev-lang/go:1.8
	dev-lang/go:1.9
	dev-lang/go:1.10
	dev-lang/go:1.11
	dev-lang/go-bootstrap
	dev-libs/dbus-glib
	>=dev-libs/glib-2.26.1
	dev-libs/libgcrypt
	dev-libs/libxslt
	dev-libs/libyaml
	dev-libs/protobuf
	dev-python/ctypesgen
	dev-python/mako
	sys-devel/bc
	dev-util/gdbus-codegen
	dev-util/gperf
	>=dev-util/gtk-doc-am-1.13
	>=dev-util/intltool-0.30
	dev-util/scons
	dev-vcs/cvs
	>=dev-vcs/git-1.7.2
	dev-vcs/mercurial
	dev-vcs/subversion[-dso]
	net-misc/google-cloud-sdk
	sys-apps/usbutils
	sys-apps/systemd
	!sys-apps/nih-dbus-tool
	=sys-devel/automake-1.10*
	sys-libs/libnih
	sys-libs/nss-usrfiles
	sys-power/iasl
	virtual/udev
	app-text/asciidoc
	app-text/xmlto
	sys-apps/gptfdisk
	net-libs/libtirpc
	"

# Host dependencies that create usernames/groups we need to pull over to target.
RDEPEND="${RDEPEND}
	sys-apps/dbus
	"

# Host dependencies that are needed by mod_image_for_test.
RDEPEND="${RDEPEND}
	sys-process/lsof
	"

# Useful utilities for developers.
RDEPEND="${RDEPEND}
	app-arch/zip
	app-portage/eclass-manpages
	app-portage/gentoolkit
	app-portage/portage-utils
	app-editors/vim
	dev-util/perf
	sys-apps/pv
	app-shells/bash-completion
	"

# Host dependencies that are needed to create and sign images
# TODO:	sys-apps/mosys
RDEPEND="${RDEPEND}
	sys-fs/squashfs-tools
	"

# Host dependencies that are needed for delta_generator.
RDEPEND="${RDEPEND}
	coreos-base/update_engine
	"

# Host dependencies for running pylint within the chroot
# TODO: move to sdk-extras
RDEPEND="${RDEPEND}
	dev-python/pylint
	"

# Host dependencies to scp binaries from the binary component server
# TODO: chromeos-base/ssh-known-hosts
#	chromeos-base/ssh-root-dot-dir
RDEPEND="${RDEPEND}
	net-misc/openssh
	net-misc/wget
	"

# Host dependencies for building ISOs
RDEPEND="${RDEPEND}
	virtual/cdrtools
	"

# Uninstall these packages.
RDEPEND="${RDEPEND}
	!net-misc/dhcpcd
	!coreos-base/google-breakpad
	"
