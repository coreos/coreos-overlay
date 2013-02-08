# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="2"
inherit eutils flag-o-matic python toolchain-funcs

DESCRIPTION="The Mozc engine for IBus Framework"
HOMEPAGE="http://code.google.com/p/mozc"
SRC_URI="http://commondatastorage.googleapis.com/chromeos-localmirror/distfiles/mozc-${PV}.tar.bz2
internal? ( gs://chromeos-localmirror-private/distfiles/GoogleJapaneseInputFilesForChromeOS-${PV}.tar.bz2 )"
LICENSE="BSD"
IUSE="internal"
# TODO(nona): Remove libcurl dependency.
RDEPEND=">=app-i18n/ibus-1.4.1
         dev-libs/openssl
         dev-libs/protobuf
         internal? (
                 net-misc/curl
         )"
DEPEND="${RDEPEND}"
SLOT="0"
KEYWORDS="amd64 x86 arm"
BUILDTYPE="${BUILDTYPE:-Release}"
RESTRICT="mirror"

src_configure() {
  if use internal; then
    BRANDING="${BRANDING:-GoogleJapaneseInput}"
    SIZE_LIMIT=20000000
  else
    BRANDING="${BRANDING:-Mozc}"
    SIZE_LIMIT=25000000
  fi

  cd "mozc-${PV}" || die
  # Generate make files
  export GYP_DEFINES="chromeos=1 use_libzinnia=0"
  export BUILD_COMMAND="emake"

  $(PYTHON) build_mozc.py gyp --gypdir="third_party/gyp" \
      --target_platform="ChromeOS" \
      --use_libprotobuf \
      --branding="${BRANDING}" || die
}

src_prepare() {
  if use internal; then
    einfo "Building Google Japanese Input for ChromeOS"
    rm -fr "mozc-${PV}/data/dictionary" || die
    rm -fr "mozc-${PV}/dictionary" || die
    rm -f "mozc-${PV}/mozc_version_template.txt" || die
    mv  "data/dictionary" "mozc-${PV}/data/" || die
    mv  "dictionary" "mozc-${PV}/" || die
    mv  "mozc_version_template.txt" "mozc-${PV}/" || die
    # Reduce a binary size.
    # TODO(hsumita): Remove this patch when it becomes a default behavior of
    # Mozc for ChromeOS.
    rm -f "mozc-${PV}/converter/converter_base.gyp" || die
    mv  "converter/converter_base.gyp" "mozc-${PV}/converter/" || die
  else
    einfo "Building Mozc for ChromiumOS"
  fi

  # Remove the patch when new mozc is released.
  epatch "${FILESDIR}"/${P}-attachment-cleanup.patch || die
  epatch "${FILESDIR}"/${P}-handle-extra-keysyms.patch || die
}

src_compile() {
  cd "mozc-${PV}" || die
  # Create build tools for the host platform.
  CFLAGS="" CXXFLAGS="" $(PYTHON) build_mozc.py build_tools -c ${BUILDTYPE} \
      || die

  # Build artifacts for the target platform.
  tc-export CXX CC AR AS RANLIB LD
  $(PYTHON) build_mozc.py build unix/ibus/ibus.gyp:ibus_mozc -c ${BUILDTYPE} \
      || die
}

src_install() {
  cd "mozc-${PV}" || die
  exeinto /usr/libexec || die
  newexe "out_linux/${BUILDTYPE}/ibus_mozc" ibus-engine-mozc || die

  insinto /usr/share/ibus/component || die
  doins out_linux/${BUILDTYPE}/obj/gen/unix/ibus/mozc.xml || die

  cp "out_linux/${BUILDTYPE}/ibus_mozc" "${T}" || die
  $(tc-getSTRIP) --strip-unneeded "${T}"/ibus_mozc || die

  # Check the binary size to detect binary size bloat (which happend once due
  # typos in .gyp files).
  test `stat -c %s "${T}"/ibus_mozc` -lt ${SIZE_LIMIT} \
      || die "The binary size of mozc for Japanese is too big (more than ~${SIZE_LIMIT})"
  rm -f "${T}"/ibus_mozc
}
