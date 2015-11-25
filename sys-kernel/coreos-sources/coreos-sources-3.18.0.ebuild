# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=5

ETYPE=sources
K_SECURITY_UNSUPPORTED=1
inherit kernel-2
detect_version
detect_arch

inherit git-2 versionator
EGIT_REPO_URI=https://github.com/96boards/linux.git
EGIT_PROJECT="hikey-linux.git"
EGIT_COMMIT="96boards-hikey-15.06"

DESCRIPTION="96 HiKey kernel sources"
HOMEPAGE="https://www.96boards.org/"

KEYWORDS="arm64"
