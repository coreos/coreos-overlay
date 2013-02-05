# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

#
# Original Author: The Chromium OS Authors <chromium-os-dev@chromium.org>
# Purpose: Eclass for handling autotest deps packages
#

EAPI=2

inherit autotest


AUTOTEST_CONFIG_LIST=""
AUTOTEST_DEPS_LIST=""
AUTOTEST_PROFILERS_LIST=""

#
# In order to build only deps (call their setup function), we need to have
# a test that calls their setup() in its own setup(). This is done by
# creating a "fake" test, prebuilding it, and then deleting it after install.
#

AUTOTEST_FORCE_TEST_LIST="myfaketest"

autotest-deponly_src_prepare() {
	autotest_src_prepare

	pushd "${AUTOTEST_WORKDIR}/client/site_tests/" 1> /dev/null || die
	mkdir myfaketest
	cd myfaketest

	# NOTE: Here we create a fake test case, that does not do anything except for
	# setup of all deps.
cat << ENDL > myfaketest.py
from autotest_lib.client.bin import test, utils

class myfaketest(test.test):
  def setup(self):
ENDL

	for item in ${AUTOTEST_DEPS_LIST}; do
echo "    self.job.setup_dep(['${item}'])" >> myfaketest.py
	done

	chmod a+x myfaketest.py
	popd 1> /dev/null
}

autotest-deponly_src_install() {
	autotest_src_install

	rm -rf ${D}/usr/local/autotest/client/site_tests/myfaketest || die
}

EXPORT_FUNCTIONS src_prepare src_install
