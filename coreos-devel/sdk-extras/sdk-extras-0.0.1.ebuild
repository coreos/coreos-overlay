# Copyright 2017 The CoreOS Authors
# Distributed under the terms of the GNU General Public License v2

# This isn't meant to be installed directly. It mostly exists so we can better keep track of
# dependencies within the SDK.

EAPI=5

DESCRIPTION="Meta ebuild for everything that isn't needed in the SDK, but might be useful"
HOMEPAGE="http://coreos.com/docs/sdk/"
SRC_URI=""

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

RDEPEND="
	app-admin/python-updater
	app-backup/casync
	app-crypt/efitools
	app-crypt/tpm-tools
	app-editors/emacs
	app-editors/nano
	app-portage/eix
	app-portage/gentoolkit-dev
	app-portage/repoman
	app-misc/screen
	app-misc/tmux
	app-text/tree
	app-text/dos2unix
	coreos-devel/fero-client
	dev-util/cscope
	dev-util/perf
	dev-util/strace
	dev-util/valgrind
	dev-go/glide
	dev-go/godep
	dev-python/awscli
	sys-apps/ed
"

# Needed for Jenkins
RDEPEND+="
	dev-util/catalyst
"
