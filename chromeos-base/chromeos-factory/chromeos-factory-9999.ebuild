# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_PROJECT=("chromiumos/platform/factory" "chromiumos/platform/installer")
CROS_WORKON_LOCALNAME=("factory" "installer")
CROS_WORKON_DESTDIR=("${S}" "${S}/installer")

inherit cros-workon
inherit cros-binary
inherit python

DESCRIPTION="Chrome OS Factory Tools and Data"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""
LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE="+autotest +build_tests"

DEPEND="chromeos-base/chromeos-chrome
	dev-python/pyyaml"
RDEPEND="!chromeos-base/chromeos-factorytools
	chromeos-base/chromeos-factory-board
	dev-lang/python
	dev-python/argparse
	dev-python/jsonrpclib
	dev-python/netifaces
	dev-python/pyyaml
	dev-python/setproctitle
	dev-util/stressapptest
	>=chromeos-base/vpd-0.0.1-r11"

CROS_WORKON_LOCALNAME="factory"

TARGET_DIR="/usr/local/factory"

CROS_BINARY_URI="http://commondatastorage.googleapis.com/chromeos-localmirror/distfiles/closure-library-20111110-r1376.tar.bz2"
CROS_BINARY_SUM="761af448631b4dd2339e01b04cb11140ad6d7706"

src_unpack() {
	cros-workon_src_unpack
	cros-binary_src_unpack
}

src_compile() {
	emake CLOSURE_LIB_ARCHIVE="${CROS_BINARY_STORE_DIR}/${CROS_BINARY_URI##*/}"
}

src_install() {
	emake DESTDIR="${D}" TARGET_DIR="${TARGET_DIR}" \
		PYTHON_SITEDIR="${EROOT}/$(python_get_sitedir)" \
		PYTHON="$(PYTHON)" \
		par install

	dosym ../../../../local/factory/py $(python_get_sitedir)/cros/factory

	# Replace chromeos-common.sh symlink with the real file
	cp --remove-destination "${S}/installer/chromeos-common.sh" \
		"${D}${TARGET_DIR}/bundle/factory_setup/lib/chromeos-common.sh" || die

	if use autotest && use build_tests; then
		# For now, point 'custom' to suite_Factory.  TODO(jsalz): Actually
		# install files directly into custom as appropriate.
		dosym ../autotest/client/site_tests/suite_Factory ${TARGET_DIR}/custom
		# We need to preserve the chromedriver and selenium library
		# (from chromeos-chrome pyauto test folder which is stripped by default)
		# for factory test images.
		local pyauto_path="/usr/local/autotest/client/deps/pyauto_dep"
		exeinto "$TARGET_DIR/bin/"
		doexe "${ROOT}$pyauto_path/test_src/out/Release/chromedriver"
		insinto "$TARGET_DIR/py/automation"
		doins -r "${ROOT}$pyauto_path/test_src/third_party/webdriver/pylib/selenium"
	fi

	# Directories used by Goofy.
	keepdir /var/factory/{,log,state,tests}
}

pkg_postinst() {
	python_mod_optimize ${TARGET_DIR}/py
	# Sanity check: make sure we can import stuff with only the
	# .par file.
	PYTHONPATH="${EROOT}/${TARGET_DIR}/bundle/shopfloor/factory.par" \
		"$(PYTHON)" -c "import cros.factory.test.state" || die
}
