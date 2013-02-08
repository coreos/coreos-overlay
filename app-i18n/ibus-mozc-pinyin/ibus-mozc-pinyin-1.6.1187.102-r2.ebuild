# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="2"
inherit eutils flag-o-matic python toolchain-funcs

DESCRIPTION="The Mozc Pinyin engine for IBus Framework"
HOMEPAGE="http://code.google.com/p/mozc"
SRC_URI="http://commondatastorage.googleapis.com/chromeos-localmirror/distfiles/mozc-${PV}.tar.bz2"
LICENSE="BSD"
RDEPEND=">=app-i18n/ibus-1.3.99
	>=dev-libs/glib-2.26
	dev-libs/protobuf
	>=dev-libs/pyzy-0.1.0"
DEPEND="${RDEPEND}"
SLOT="0"
KEYWORDS="amd64 x86 arm"
BUILDTYPE="${BUILDTYPE:-Release}"

src_configure() {
	cd "mozc" || die
	# Generate make files
	export GYP_DEFINES="chromeos=1 use_libzinnia=0 use_libprotobuf=1"
	export BUILD_COMMAND="emake"

	# Currently --channel_dev=0 is not necessary for Pinyin, but just in case.
	$(PYTHON) build_mozc.py gyp --gypdir="third_party/gyp" \
		--language=pinyin \
		--noqt \
		--target_platform="ChromeOS" --channel_dev=0 || die
}

src_prepare() {
	cd "mozc" || die
	epatch "${FILESDIR}"/"${P}"-clear-key-state-on-disable.patch || die
	epatch "${FILESDIR}"/"${P}"-x32.patch
	# Following 2 patches are required by any version of ibus-mozc-pinyin on
	# ChromeOS
	epatch "${FILESDIR}"/ibus-mozc-pinyin-introduces-typo-for-compatibility.patch || die
	epatch "${FILESDIR}"/ibus-mozc-pinyin-replace-ibus-pinyin.patch || die
}

src_compile() {
	cd "mozc" || die
	# Create build tools for the host platform.
	CFLAGS="" CXXFLAGS="" $(PYTHON) build_mozc.py build_tools -c ${BUILDTYPE} \
		|| die

	# Build artifacts for the target platform.
	tc-export CXX CC AR AS RANLIB LD
	$(PYTHON) build_mozc.py build \
		pinyin/pinyin.gyp:ibus_mozc_pinyin -c ${BUILDTYPE} || die
}

src_install() {
	cd "mozc" || die
	exeinto /usr/libexec || die
	newexe "out_linux/${BUILDTYPE}/ibus_mozc_pinyin" ibus-engine-mozc-pinyin \
		|| die

	insinto /usr/share/ibus/component || die
	doins languages/pinyin/unix/ibus/mozc-pinyin.xml || die

	mkdir -p "${D}"/usr/share/ibus-mozc-pinyin || die
	cp "out_linux/${BUILDTYPE}/ibus_mozc_pinyin" "${T}" || die
	$(tc-getSTRIP) --strip-unneeded "${T}"/ibus_mozc_pinyin || die

	# Unnecessary link may cause size bloat. We expect to detect it by checking binary size.
	# NOTE: The binary size for amd64 is 1.1MB in Apr. 2012
	FILESIZE=`stat -c %s "${T}"/ibus_mozc_pinyin`
	einfo "The binary size is ${FILESIZE}"
	test ${FILESIZE} -lt 2000000 \
		|| die 'The binary size of ibus_mozc_pinyin is too big (more than ~2.0MB)'
	rm -f "${T}"/ibus_mozc_pinyin
}
