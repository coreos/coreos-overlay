# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

# @ECLASS: cros-workon.eclass
# @BLURB: helper eclass for building CoreOS SDK provided source
# @DESCRIPTION:
# Instead of cloning git trees directly from github ebuilds can fetch
# from the local cache maintained by repo. Additionally live builds can
# build directly from the source checked out in the repo tree.

# @ECLASS-VARIABLE: CROS_WORKON_REPO
# @DESCRIPTION:
# Git URL which is prefixed to CROS_WORKON_PROJECT
: "${CROS_WORKON_REPO:=https://chromium.googlesource.com}"

# @ECLASS-VARIABLE: CROS_WORKON_PROJECT
# @DESCRIPTION:
# Git project name which is suffixed to CROS_WORKON_REPO
: "${CROS_WORKON_PROJECT:=${PN}}"

# @ECLASS-VARIABLE: CROS_WORKON_LOCALDIR
# @DESCRIPTION:
# Repo checkout directory which is prefixed to CROS_WORKON_LOCALNAME
# Generally is either src/third_party or src/platform
: "${CROS_WORKON_LOCALDIR:=src/third_party}"

# @ECLASS-VARIABLE: CROS_WORKON_LOCALNAME
# @DESCRIPTION:
# Directory name which is suffixed to CROS_WORKON_LOCALDIR
: "${CROS_WORKON_LOCALNAME:=${PN}}"

# @ECLASS-VARIABLE: CROS_WORKON_COMMIT
# @DESCRIPTION:
# Git commit to checkout to
: "${CROS_WORKON_COMMIT:=}"

# @ECLASS-VARIABLE: CROS_WORKON_TREE
# @DESCRIPTION:
# SHA1 of the contents of the repository. This is used for verifying the
# correctness of prebuilts. Unlike the commit hash, this SHA1 is unaffected
# by the history of the repository, or by commit messages.
: "${CROS_WORKON_TREE:=}"

# @ECLASS-VARIABLE: CROS_WORKON_SRCROOT
# @DESCRIPTION:
# Directory where chrome third party and platform sources are located (formerly CHROMEOS_ROOT)
: "${CROS_WORKON_SRCROOT:=}"

# TODO(marineam): Remove profiling and cros_workon_tree_* use flags.
# These have never been used in CoreOS.
IUSE="cros_workon_tree_$CROS_WORKON_TREE profiling"

inherit git-r3

# Calculate path where code should be cloned from.
get_path() {
	local pathbase
	if [[ -n "${CROS_WORKON_SRCROOT}" ]]; then
		pathbase="${CROS_WORKON_SRCROOT}"
	elif [[ -n "${CHROMEOS_ROOT}" ]]; then
		pathbase="${CHROMEOS_ROOT}"
	else
		# HACK: Figure out the missing legacy path for now
		# this only happens in amd64 chroot with sudo emerge.
		pathbase="/mnt/host/source"
	fi

	echo "${pathbase}/${CROS_WORKON_LOCALDIR}/${CROS_WORKON_LOCALNAME}"
}

local_copy() {
	debug-print-function ${FUNCNAME} "$@"

	local path="$(get_path)"

	einfo "Cloning ${path}"
	git clone -sn "${path}" "${S}" || die "Can't clone ${path}"

	einfo "Copying source from ${path}"
	rsync -a --exclude=.git "${path}/" "${S}" || return 1
}

local_clone() {
	debug-print-function ${FUNCNAME} "$@"

	local path="$(get_path)"
	if [[ ! -d "${path}/.git" ]]; then
		return 1
	fi

	einfo "Cloning ${path}"
	einfo "Checking out ${CROS_WORKON_COMMIT}"

	# Looks like we already have a local copy of the repository.
	# Let's use these and checkout ${CROS_WORKON_COMMIT}.
	#  -s: For speed, share objects between ${path} and ${S}.
	#  -n: Don't checkout any files from the repository yet. We'll
	#      checkout the source separately.
	#
	# We don't use git clone to checkout the source because the -b
	# option for clone defaults to HEAD if it can't find the
	# revision you requested. On the other hand, git checkout fails
	# if it can't find the revision you requested, so we use that
	# instead.

	git clone -sn "${path}" "${S}" || die "Can't clone ${path}"
	if ! git -C "${S}" checkout -q "${CROS_WORKON_COMMIT}"; then
		ewarn "Cannot run git checkout ${CROS_WORKON_COMMIT} in ${S}."
		ewarn "Is ${path} up to date? Try running repo sync."
		rm -rf "${S}" || die
		return 1
	fi
}

cros-workon_src_unpack() {
	debug-print-function ${FUNCNAME} "$@"

	# Sanity check.  We cannot have S set to WORKDIR because if/when we try
	# to check out repos, git will die if it tries to check out into a dir
	# that already exists.  Some packages might try this when out-of-tree
	# builds are enabled, and they'll work fine most of the time because
	# they'll be using a full manifest and will just re-use the existing
	# checkout in src/platform/*.  But if the code detects that it has to
	# make its own checkout, things fall apart.  For out-of-tree builds,
	# the initial $S doesn't even matter because it resets it below to the
	# repo in src/platform/.
	if [[ ${S} == "${WORKDIR}" ]]; then
		die "Sorry, but \$S cannot be set to \$WORKDIR"
	fi

	# Hack
	# TODO(msb): remove once we've resolved the include path issue
	# http://groups.google.com/a/chromium.org/group/chromium-os-dev/browse_thread/thread/5e85f28f551eeda/3ae57db97ae327ae
	ln -s "${S}" "${WORKDIR}/${CROS_WORKON_LOCALNAME}" &> /dev/null

	if [[ "${PV}" == "9999" ]]; then
		local_copy || die "Cannot create a local copy"
	else
		if [[ -z "${CROS_WORKON_COMMIT}" ]]; then
			die "CROS_WORKON_COMMIT is unset"
		fi

		# Try cloning from a local repo first
		local_clone && return

		# There is no good way to ensure repo-maintained git trees
		# always have exactly what ebuilds need so fall back gracefully
		# to fetching remotely. Also, when ebuilds only specify a git
		# hash there isn't a way to know which branch that is on. To be
		# safe use git-r3's "mirror" mode to fetch all remote branches.
		ewarn "Falling back to fetching from remote git repository..."

		EGIT_CLONE_TYPE=mirror
		EGIT_REPO_URI="${CROS_WORKON_REPO}/${CROS_WORKON_PROJECT}.git"
		EGIT_CHECKOUT_DIR="${S}"
		EGIT_COMMIT="${CROS_WORKON_COMMIT}"
		git-r3_src_unpack
	fi
}

# TODO(marineam): This is used by cros_workon to deal manage
# 'minilayout' manifests but we don't bother with that in CoreOS.
# Do a pass on the cros_workon tool to give it a freshening too...
cros-workon_pkg_info() {
	echo "CROS_WORKON_SRCDIR=(\"$(get_path)\")"
	echo "CROS_WORKON_PROJECT=(\"${CROS_WORKON_PROJECT}\")"
}

# get the semver of the git repo that is being built
get_semver() {
	# get git tag/sha
	local v
	v=$(git describe --long --dirty) || die

	# strip a leading v from the tag
	v=${v#v}

	# replace first - with +, to attach git sha/dirty as semver metadata
	v=${v/-/+}

	echo ${v}
}

EXPORT_FUNCTIONS src_unpack pkg_info

