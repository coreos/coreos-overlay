# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

# for function yesno
. /lib/udev/shell-compat-addon.sh

# store device tarball
(
	. /etc/init.d/udev-dev-tarball
	stop
)

exit 0
