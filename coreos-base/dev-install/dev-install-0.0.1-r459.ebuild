# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

# This ebuild file installs the developer installer package. It:
#  + Copies dev_install.
#  + Copies some config files for emerge: make.defaults and make.conf.
#  + Generates a list of packages installed (in base images).
# dev_install downloads and bootstraps emerge in base images without
# modifying the root filesystem.

EAPI="4"
CROS_WORKON_COMMIT="597ca1198129fab4b870618c74ae4d51b6b85e4a"
CROS_WORKON_TREE="ae6f2543c17e05c66b249852d15090e66a96c45f"
CROS_WORKON_PROJECT="chromiumos/platform/dev-util"
CROS_WORKON_LOCALNAME="dev"
CROS_WORKON_OUTOFTREE_BUILD="1"

inherit cros-workon cros-board multiprocessing

DESCRIPTION="Chromium OS Developer Packages installer"
HOMEPAGE="http://www.chromium.org/chromium-os"

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="cros-debug"

DEPEND="app-arch/tar
	sys-apps/coreutils
	sys-apps/grep
	dev-util/strace
	sys-apps/portage
	sys-apps/sed"
# TODO(arkaitzr): remove dependency on tar if it's gonna be removed from the
# base image. Also modify dev_install.
RDEPEND="app-arch/tar
	net-misc/curl
	sys-apps/coreutils"

src_prepare() {
	SRCDIR="${S}/dev-install"
	mkdir -p "$(cros-workon_get_build_dir)"
}

src_compile() {
	cd "$(cros-workon_get_build_dir)"

	local useflags pkg pkgs BOARD=$(get_current_board_with_variant)

	# We need to pass down cros-debug automatically because this is often
	# times toggled at the ./build_packages level.  This is a hack of sorts,
	# but covers the most common case.
	useflags="${USE}"
	use cros-debug || useflags+=" -cros-debug"

	pkgs=(
		# Generate a list of packages that go into the base image. These
		# packages will be assumed to be installed by emerge in the target.
		coreos

		# Get the list of the packages needed to bootstrap emerge.
		portage

		# Get the list of dev and test packages.
		coreos-dev
		coreos-test
	)
	einfo "Ignore warnings below related to LD_PRELOAD/libsandbox.so"
	multijob_init
	for pkg in ${pkgs[@]} ; do
		# The ebuild env will modify certain variables in ways that we
		# do not care for.  For example, PORTDIR_OVERLAY is modified to
		# only point to the current tree which screws up the search of
		# the board-specific overlays.
		(
		multijob_child_init
		env -i PATH="${PATH}" PORTAGE_USERNAME="${PORTAGE_USERNAME}" USE="${useflags}" \
		emerge-${BOARD} \
			--pretend --quiet --emptytree --ignore-default-opts \
			--root-deps=rdeps ${pkg} | \
			egrep -o ' [[:alnum:]-]+/[^[:space:]/]+\b' | \
			tr -d ' ' | \
			sort > ${pkg}.packages
		_pipestatus=${PIPESTATUS[*]}
		[[ ${_pipestatus// } -eq 0 ]] || die "\`emerge-${BOARD} ${pkg}\` failed"
		) &
		multijob_post_fork
	done
	multijob_finish
	# No virtual packages in package.provided. We store packages for
	# package.provided in file coreos-base.packages as package.provided is a
	# directory.
	grep -v "virtual/" coreos.packages > coreos-base.packages

	python "${FILESDIR}"/filter.py || die

	# Add the board specific binhost repository.
	sed -e "s|BOARD|${BOARD}|g" "${SRCDIR}/repository.conf" > repository.conf

	# Add dhcp to the list of packages installed since its installation will not
	# complete (can not add dhcp group since /etc is not writeable). Bootstrap it
	# instead.
	grep "net-misc/dhcp-" coreos-dev.packages >> coreos-base.packages
	grep "net-misc/dhcp-" coreos-dev.packages >> bootstrap.packages
}

src_install() {
	local build_dir=$(cros-workon_get_build_dir)

	cd "${SRCDIR}"
	dobin dev_install

	insinto /usr/share/${PN}/portage
	doins "${build_dir}"/{bootstrap.packages,repository.conf}

	insinto /usr/share/${PN}/portage/make.profile
	doins "${build_dir}"/package.installable make.{conf,defaults}

	insinto /usr/share/${PN}/portage/make.profile/package.provided
	doins "${build_dir}"/coreos-base.packages

	insinto /etc/env.d
	doins 99devinstall
}

pkg_preinst() {
	if [[ $(cros_target) == "target_image" ]]; then
		# We don't want to install these files into the normal /build/
		# dir because we need different settings at build time vs what
		# we want at runtime in release images.  Thus, install the files
		# into /usr/share but symlink them into /etc for the images.
		local f srcdir="/usr/share/${PN}"
		pushd "${ED}/${srcdir}" >/dev/null
		for f in $(find -type f -printf '%P '); do
			dosym "${srcdir}/${f}" "/etc/${f}"
		done
		popd >/dev/null
	fi
}
