# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_PROJECT="chromiumos/third_party/autotest"

inherit toolchain-funcs flag-o-matic cros-workon

DESCRIPTION="Autotest scripts and tools"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""
LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~x86 ~arm ~amd64"

RDEPEND="
	!<chromeos-base/autotest-chrome-0.0.1-r1788
	!<chromeos-base/autotest-tests-0.0.1-r3291
"

# We don't want Python on the base image, however, there're several base
# chromeos dependent ebuilds that depend on this ebuild.
DEPEND="${RDEPEND}"

# Ensure the configures run by autotest pick up the right config.site
export CONFIG_SITE=/usr/share/config.site

CROS_WORKON_LOCALNAME=../third_party/autotest
CROS_WORKON_SUBDIR=files
# Block AFE runtime data and test data that is not useful here.

# Note - we specifically copy over all server-side tests until crosbug.com/35194
# is resolved.
CROS_WORKON_SUBDIR_BLACKLIST=( "packages" "results" "site-packages" "frontend/client/www" client/{tests,site_tests} )

AUTOTEST_WORK="${WORKDIR}/autotest-work"

src_prepare() {
	mkdir -p "${AUTOTEST_WORK}/client"
	mkdir -p "${AUTOTEST_WORK}/server"
	cp -fpu "${S}"/client/* "${AUTOTEST_WORK}/client" &>/dev/null
	cp -fpru "${S}"/client/{bin,common_lib,tools} "${AUTOTEST_WORK}/client"
	cp -fpu "${S}"/server/* "${AUTOTEST_WORK}/server" &>/dev/null
	cp -fpru "${S}"/server/{bin,control_segments,hosts,site_tests,tests} \
    "${AUTOTEST_WORK}/server"
	cp -fpru "${S}"/{conmux,tko,utils,site_utils,test_suites,frontend} "${AUTOTEST_WORK}"

	# cros directory is not from autotest upstream but cros project specific.
	cp -fpru "${S}"/client/cros "${AUTOTEST_WORK}/client"

	cp -fpru "${S}"/server/cros "${AUTOTEST_WORK}/server"

	# Pre-create test directories.
	local test_dirs="
		client/tests client/site_tests
		client/config client/deps client/profilers
		packages"
	local dir
	for dir in ${test_dirs}; do
		mkdir "${AUTOTEST_WORK}/${dir}"
		touch "${AUTOTEST_WORK}/${dir}"/.keep
	done

	sed "/^enable_server_prebuild/d" "${S}/global_config.ini" > \
		"${AUTOTEST_WORK}/global_config.ini"
}

src_install() {
	insinto /usr/local/autotest
	doins -r "${AUTOTEST_WORK}"/*

	# base __init__.py
	touch "${D}"/usr/local/autotest/__init__.py

	# TODO: This should be more selective
	chmod -R a+x "${D}"/usr/local/autotest

	# setup stuff needed for read/write operation
	chmod a+wx "${D}/usr/local/autotest/packages"

	dodir "/usr/local/autotest/client/packages"
	chmod a+wx "${D}/usr/local/autotest/client/packages"

	dodir "/usr/local/autotest/server/tmp"
	chmod a+wx "${D}/usr/local/autotest/server/tmp"

	# Set up symlinks so that debug info works for autotests.
	dodir /usr/lib/debug/usr/local/autotest/
	dosym client/site_tests /usr/lib/debug/usr/local/autotest/tests
}

# Packages client.
pkg_postinst() {
	local root_autotest_dir="${ROOT}/usr/local/autotest"
	flock "${root_autotest_dir}/packages" \
			-c "python -B ${root_autotest_dir}/utils/packager.py \
				-r ${root_autotest_dir}/packages --client upload"
}
