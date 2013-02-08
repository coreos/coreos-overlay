# Copyright (c) 2010 The Chromium OS Authors.  All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header$

EAPI="2"
CROS_WORKON_COMMIT="1814ad708d10f34ad8f03d3a067d72bc32782e3c"
CROS_WORKON_TREE="c131259845e91d4cd0fd8001c0598d49f59c4b7d"
CROS_WORKON_PROJECT="chromiumos/platform/tpm"

inherit cros-workon autotools
inherit cros-workon base
inherit cros-workon eutils
inherit cros-workon linux-info

DESCRIPTION="Various TPM tools"
LICENSE="BSD"
HOMEPAGE="http://www.chromium.org/"
SLOT="0"
KEYWORDS="amd64 arm x86"

DEPEND="app-crypt/trousers"

CROS_WORKON_LOCALNAME="../third_party/tpm"

src_compile() {
  if tc-is-cross-compiler ; then
    tc-getCC
    tc-getCXX
    tc-getAR
    tc-getRANLIB
    tc-getLD
    tc-getNM
    export PKG_CONFIG_PATH="${ROOT}/usr/lib/pkgconfig/"
    export CCFLAGS="$CFLAGS"
  fi
  (cd nvtool; emake) || die emake failed
}

src_install() {
  exeinto /usr/bin
  doexe nvtool/tpm-nvtool
}
