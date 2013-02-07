# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2

# CROS_WORKON_REPO=FILL_YOUR_REPO_URL_HERE
# inherit toolchain-funcs flag-o-matic cros-workon autotest

DESCRIPTION="Private autotest tests"
HOMEPAGE="http://src.chromium.org"
SRC_URI=""
LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm x86"
DEPEND=""
RDEPEND=""

# This ebuild file is reserved for adding new private tests in your factory
# process. You can change the CROS_WORKON_REPO to your own server, and uncomment
# the following CROS_WORKON_* variables to have your own tests merged when
# building factory test run-in images.

# CROS_WORKON_PROJECT=autotest-private
# CROS_WORKON_LOCALNAME=../third_party/autotest-private
# CROS_WORKON_SUBDIR=
