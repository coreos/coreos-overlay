# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_PROJECT="coreos/dev-util"
CROS_WORKON_REPO="git://github.com"
CROS_WORKON_LOCALNAME="dev"
CROS_WORKON_LOCALDIR="src/platform"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm ~x86"
else
	CROS_WORKON_COMMIT="072c33135839b692c6ceb37765e2e0f1a65b416c"
	KEYWORDS="amd64 arm x86"
fi

inherit cros-workon multilib python-utils-r1

DESCRIPTION="Development utilities for CoreOS"
HOMEPAGE="https://github.com/coreos/dev-util"

LICENSE="GPL-2"
SLOT="0"
IUSE="cros_host test"

RDEPEND="cros_host? ( app-emulation/qemu )
	app-portage/gentoolkit
	cros_host? ( app-shells/bash )
	!cros_host? ( !chromeos-base/gmerge )
	dev-lang/python
	dev-util/shflags
	cros_host? ( dev-util/crosutils )
	app-crypt/efitools
	!coreos-base/coreos-installer
	"
# These are all either bash / python scripts.  No actual builds DEPS.
DEPEND=""

src_install() {
	# Run the devserver Makefile.
	emake install DESTDIR="$D"

	dosym /build /var/lib/devserver/static/pkgroot
	dosym /var/lib/devserver/static /usr/lib/devserver/static

	if ! use cros_host; then
		dobin gmerge stateful_update
	else
		local host_tools
		host_tools=(
			cros_bundle_firmware
			cros_choose_profile
			cros_chrome_make
			cros_fetch_image
			cros_sign_bootstub
			cros_start_vm
			cros_stop_vm
			cros_workon
			cros_workon_make
			cros_write_firmware
			dump_i2c
			dump_tpm
			gdb_remote
			gdb_x86_local
			gmergefs
			image_to_live.sh
			strip_package
			ssh_no_update
			willis
		)

		dobin "${host_tools[@]/#/host/}"

		# Payload generation scripts.
		dobin host/cros_generate_update_payload

		# Repo and git bash completion.
		insinto /usr/share/bash-completion
		newins host/repo_bash_completion repo
		dosym /usr/share/bash-completion/git /etc/bash_completion.d/git
		dosym /usr/share/bash-completion/repo /etc/bash_completion.d/repo

		insinto "$(python_get_sitedir python2_7 PYTHON)"
		doins host/lib/*.py

		insinto "/usr/lib/crosutils"
		doins host/lib/cros_archive.sh
	fi
}

src_test() {
	cd "${S}" # Let's just run unit tests from ${S} rather than install and run.

	# Run bundle_firmware tests
	pushd host/lib >/dev/null
	local libfile
	for libfile in *.py; do
		einfo Running tests in ${libfile}
		python ${libfile} --test || \
			die "Unit tests failed at ${libfile}!"
	done
	popd >/dev/null

	pushd host/tests >/dev/null
	for ut_file in *.py; do
		echo Running tests in ${ut_file}
		PYTHONPATH=../lib python ${ut_file} ||
		die "Unit tests failed at ${ut_file}!"
	done
	popd >/dev/null

	local TESTS=()
	if ! use cros_host; then
		TESTS+=( gmerge_test.py )
		# FIXME(zbehan): import gmerge in gmerge_test.py won't work if we won't
		# have the .py.
		ln -sf gmerge gmerge.py
	else
		TESTS+=( autoupdate_unittest.py )
		TESTS+=( builder_test.py )
		TESTS+=( devserver_test.py )
		TESTS+=( common_util_unittest.py )
		TESTS+=( host/lib/cros_archive_unittest.sh )
		#FIXME(zbehan): update_test.py doesn't seem to work right now.
	fi

	local test
	for test in ${TESTS[@]} ; do
		einfo "Running ${test}"
		./${test} || die "Failed in ${test}"
	done
}

pkg_preinst() {
	# Handle pre-existing possibly problematic configurations of static
	use cros_host || return 0
	if [[ -e ${ROOT}/usr/bin/static && ! -L ${ROOT}/usr/bin/static ]] ; then
		einfo "/usr/bin/static detected, and is not a symlink, performing cleanup"
		# Well, I don't know what else should be done about it. Moving the
		# files has several issues: handling of all kinds of links, special
		# files, permissions, etc. Autoremval is not a good idea, what if
		# this ended up with accidental destruction of the system?
		local newname="static-old-${RANDOM}" # In case this happens more than once.
		mv "${ROOT}"/usr/bin/static "${ROOT}"/usr/bin/${newname}
		ewarn "/usr/bin/${newname} has the previous contents of static."
		ewarn "It can be safely deleted (or kept around forever)."
	fi
}
