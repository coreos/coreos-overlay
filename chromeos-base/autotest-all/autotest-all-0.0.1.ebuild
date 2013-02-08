# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4

DESCRIPTION="Meta ebuild for all packages providing tests"
HOMEPAGE="http://www.chromium.org"

LICENSE="GPL-2"
SLOT=0
KEYWORDS="amd64 arm x86"
IUSE=""

RDEPEND="
	chromeos-base/autotest-tests
	chromeos-base/autotest-tests-ltp
	chromeos-base/autotest-tests-ownershipapi
	chromeos-base/autotest-chrome
	chromeos-base/autotest-factory
	chromeos-base/autotest-private
"

DEPEND="${RDEPEND}"

SUITE_DEPENDENCIES_FILE="dependency_info"

src_unpack() {
	elog "Unpacking..."
	mkdir -p "${S}"
	touch "${S}/${SUITE_DEPENDENCIES_FILE}"
}

src_install() {
	# So that this package properly owns the file
	insinto /usr/local/autotest/test_suites
	doins "${SUITE_DEPENDENCIES_FILE}"
}

# Pre-processes control files and installs DEPENDENCIES info.
pkg_postinst() {
	local root_autotest_dir="${ROOT}/usr/local/autotest"
	python -B "${root_autotest_dir}/site_utils/suite_preprocessor.py" \
		-a "${root_autotest_dir}" \
		-o "${root_autotest_dir}/test_suites/${SUITE_DEPENDENCIES_FILE}"
}
