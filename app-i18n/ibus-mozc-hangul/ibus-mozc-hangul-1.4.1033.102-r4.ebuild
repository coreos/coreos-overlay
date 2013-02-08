# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="2"
inherit eutils flag-o-matic python toolchain-funcs

DESCRIPTION="The Mozc Hangul engine for IBus Framework"
HOMEPAGE="http://code.google.com/p/mozc"
SRC_URI="http://commondatastorage.googleapis.com/chromeos-localmirror/distfiles/mozc-${PV}.tar.bz2"
LICENSE="BSD"
RDEPEND=">=app-i18n/ibus-1.3.99
         >=app-i18n/libhangul-0.0.10
         dev-libs/protobuf"
DEPEND="${RDEPEND}"
SLOT="0"
KEYWORDS="amd64 x86 arm"
BUILDTYPE="${BUILDTYPE:-Release}"

src_prepare() {
  cd "mozc-${PV}" || die
  # TODO(hsumita): Remove the patch when we upgrade mozc to 1.5.
  epatch "${FILESDIR}"/${P}-arm-build-fix.patch || die
  epatch "${FILESDIR}"/${P}-consume-hanja-key.patch || die
  # TODO(hsumita): Remove the patch when we upgrade mozc to 1.8 or 1.9.
  epatch "${FILESDIR}"/${P}-handle-extra-keysyms.patch || die
}

src_configure() {
  cd "mozc-${PV}" || die
  # Generate make files
  export GYP_DEFINES="chromeos=1 use_libzinnia=0"
  export BUILD_COMMAND="emake"

  # Currently --channel_dev=0 is not neccessary for Hangul, but just in case.
  $(PYTHON) build_mozc.py gyp --gypdir="third_party/gyp" \
      --use_libprotobuf \
      --language=hangul \
      --noqt \
      --target_platform="ChromeOS" --channel_dev=0 || die
}

src_compile() {
  cd "mozc-${PV}" || die
  # Create build tools for the host platform.
  CFLAGS="" CXXFLAGS="" $(PYTHON) build_mozc.py build_tools -c ${BUILDTYPE} \
      || die

  # Build artifacts for the target platform.
  tc-export CXX CC AR AS RANLIB LD
  $(PYTHON) build_mozc.py build \
      languages/hangul/hangul.gyp:ibus_mozc_hangul -c ${BUILDTYPE} || die
}

src_install() {
  cd "mozc-${PV}" || die
  exeinto /usr/libexec || die
  newexe "out_linux/${BUILDTYPE}/ibus_mozc_hangul" ibus-engine-mozc-hangul \
      || die

  insinto /usr/share/ibus/component || die
  doins languages/hangul/unix/ibus/mozc-hangul.xml || die

  mkdir -p "${D}"/usr/share/ibus-mozc-hangul || die
  cp "${WORKDIR}/mozc-${PV}"/data/hangul/korean_symbols.txt  "${D}"/usr/share/ibus-mozc-hangul/ || die
  chmod o+r "${D}"/usr/share/ibus-mozc-hangul/korean_symbols.txt || die
  cp "out_linux/${BUILDTYPE}/ibus_mozc_hangul" "${T}" || die
  $(tc-getSTRIP) --strip-unneeded "${T}"/ibus_mozc_hangul || die

  # Check the binary size to detect binary size bloat (which happend once due
  # typos in .gyp files). Current size of the stripped ibus-mozc-hangul binary
  # is about 900k (x86) and 700k (arm).
  FILESIZE=`stat -c %s "${T}"/ibus_mozc_hangul`
  einfo "The binary size is ${FILESIZE}"
  test ${FILESIZE} -lt 1500000 \
      || die 'The binary size of mozc hangul is too big (more than ~1.5MB)'
  rm -f "${T}"/ibus_mozc_hangul
}
