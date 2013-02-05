# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/git.eclass,v 1.43 2010/02/24 01:16:35 abcd Exp $

# @ECLASS: git.eclass
# @MAINTAINER:
# Tomas Chvatal <scarabeus@gentoo.org>
# Donnie Berkholz <dberkholz@gentoo.org>
# @BLURB: This eclass provides functions for fetch and unpack git repositories
# @DESCRIPTION:
# The eclass is based on subversion eclass.
# If you use this eclass, the ${S} is ${WORKDIR}/${P}.
# It is necessary to define the EGIT_REPO_URI variable at least.
# @THANKS TO:
# Fernando J. Pereda <ferdy@gentoo.org>

inherit eutils

EGIT="git.eclass"

# We DEPEND on at least a bit recent git version
DEPEND=">=dev-vcs/git-1.6"

EXPORTED_FUNCTIONS="src_unpack"
case "${EAPI:-0}" in
	3|2) EXPORTED_FUNCTIONS="${EXPORTED_FUNCTIONS} src_prepare" ;;
	1|0) ;;
	:) DEPEND="EAPI-UNSUPPORTED" ;;
esac
EXPORT_FUNCTIONS ${EXPORTED_FUNCTIONS}

# define some nice defaults but only if nothing is set already
: ${HOMEPAGE:=http://git-scm.com/}

# @ECLASS-VARIABLE: EGIT_QUIET
# @DESCRIPTION:
# Enables user specified verbosity for the eclass elog informations.
# The user just needs to add EGIT_QUIET="ON" to the /etc/make.conf.
: ${EGIT_QUIET:="OFF"}

# @ECLASS-VARIABLE: EGIT_STORE_DIR
# @DESCRIPTION:
# Storage directory for git sources.
# Can be redefined.
[[ -z ${EGIT_STORE_DIR} ]] && EGIT_STORE_DIR="${PORTAGE_ACTUAL_DISTDIR-${DISTDIR}}/git-src"

# @ECLASS-VARIABLE: EGIT_HAS_SUBMODULES
# @DESCRIPTION:
# Set this to "true" to enable the (slower) submodule support.
# This variable should be set before inheriting git.eclass
: ${EGIT_HAS_SUBMODULES:=false}

# @ECLASS-VARIABLE: EGIT_FETCH_CMD
# @DESCRIPTION:
# Command for cloning the repository.
: ${EGIT_FETCH_CMD:="git clone"}

# @ECLASS-VARIABLE: EGIT_UPDATE_CMD
# @DESCRIPTION:
# Git fetch command.
if ${EGIT_HAS_SUBMODULES}; then
	EGIT_UPDATE_CMD="git pull -f -u"
else
	EGIT_UPDATE_CMD="git fetch -f -u"
fi

# @ECLASS-VARIABLE: EGIT_DIFFSTAT_CMD
# @DESCRIPTION:
# Git command for diffstat.
EGIT_DIFFSTAT_CMD="git --no-pager diff --stat"

# @ECLASS-VARIABLE: EGIT_OPTIONS
# @DESCRIPTION:
# This variable value is passed to clone and fetch.
: ${EGIT_OPTIONS:=}

# @ECLASS-VARIABLE: EGIT_MASTER
# @DESCRIPTION:
# Variable for specifying master branch.
# Usefull when upstream don't have master branch.
: ${EGIT_MASTER:=master}

# @ECLASS-VARIABLE: EGIT_REPO_URI
# @DESCRIPTION:
# URI for the repository
# e.g. http://foo, git://bar
# Supported protocols:
#   http://
#   https://
#   git://
#   git+ssh://
#   rsync://
#   ssh://
eval X="\$${PN//[-+]/_}_LIVE_REPO"
if [[ ${X} = "" ]]; then
	EGIT_REPO_URI=${EGIT_REPO_URI:=}
else
	EGIT_REPO_URI="${X}"
fi
# @ECLASS-VARIABLE: EGIT_PROJECT
# @DESCRIPTION:
# Project name of your ebuild.
# Git eclass will check out the git repository like:
#	${EGIT_STORE_DIR}/${EGIT_PROJECT}/${EGIT_REPO_URI##*/}
# so if you define EGIT_REPO_URI as http://git.collab.net/repo/git or
# http://git.collab.net/repo/git. and PN is subversion-git.
# it will check out like:
#	${EGIT_STORE_DIR}/subversion
: ${EGIT_PROJECT:=${PN/-git}}

# @ECLASS-VARIABLE: EGIT_BOOTSTRAP
# @DESCRIPTION:
# bootstrap script or command like autogen.sh or etc...
: ${EGIT_BOOTSTRAP:=}

# @ECLASS-VARIABLE: EGIT_OFFLINE
# @DESCRIPTION:
# Set this variable to a non-empty value to disable the automatic updating of
# an GIT source tree. This is intended to be set outside the git source
# tree by users.
EGIT_OFFLINE="${EGIT_OFFLINE:-${ESCM_OFFLINE}}"

# @ECLASS-VARIABLE: EGIT_PATCHES
# @DESCRIPTION:
# Similar to PATCHES array from base.eclass
# Only difference is that this patches are applied before bootstrap.
# Please take note that this variable should be bash array.

# @ECLASS-VARIABLE: EGIT_BRANCH
# @DESCRIPTION:
# git eclass can fetch any branch in git_fetch().
eval X="\$${PN//[-+]/_}_LIVE_BRANCH"
if [[ ${X} = "" ]]; then
	EGIT_BRANCH=${EGIT_BRANCH:=master}
else
	EGIT_BRANCH="${X}"
fi

# @ECLASS-VARIABLE: EGIT_COMMIT
# @DESCRIPTION:
# git eclass can checkout any commit.
eval X="\$${PN//[-+]/_}_LIVE_COMMIT"
if [[ ${X} = "" ]]; then
	: ${EGIT_COMMIT:=${EGIT_BRANCH}}
else
	EGIT_COMMIT="${X}"
fi

# @ECLASS-VARIABLE: EGIT_REPACK
# @DESCRIPTION:
# git eclass will repack objects to save disk space. However this can take a
# long time with VERY big repositories.
: ${EGIT_REPACK:=false}

# @ECLASS-VARIABLE: EGIT_PRUNE
# @DESCRIPTION:
# git eclass can prune the local clone. This is useful if upstream rewinds and
# rebases branches too often.
: ${EGIT_PRUNE:=false}

# @FUNCTION: git_submodules
# @DESCRIPTION:
# Internal function wrapping the submodule initialisation and update
git_submodules() {
	if ${EGIT_HAS_SUBMODULES}; then
		debug-print "git submodule init"
		git submodule init
		debug-print "git submodule update"
		git submodule update
	fi
}

# @FUNCTION: git_branch
# @DESCRIPTION:
# Internal function that changes branch for the repo based on EGIT_TREE and
# EGIT_BRANCH variables.
git_branch() {
	local branchname=branch-${EGIT_BRANCH} src=origin/${EGIT_BRANCH}
	if [[ ${EGIT_COMMIT} != ${EGIT_BRANCH} ]]; then
		branchname=tree-${EGIT_COMMIT}
		src=${EGIT_COMMIT}
	fi
	debug-print "git checkout -b ${branchname} ${src}"
	git checkout -b ${branchname} ${src} || \
		die "${EGIT}: Could not run git checkout -b ${branchname} ${src}"

	unset branchname src
}

# @FUNCTION: git_fetch
# @DESCRIPTION:
# Gets repository from EGIT_REPO_URI and store it in specified EGIT_STORE_DIR
git_fetch() {
	debug-print-function ${FUNCNAME} "$@"

	local GIT_DIR EGIT_CLONE_DIR oldsha1 cursha1 extra_clone_opts upstream_branch
	${EGIT_HAS_SUBMODULES} || export GIT_DIR

	# choose if user wants elog or just einfo.
	if [[ ${EGIT_QUIET} != OFF ]]; then
		elogcmd="einfo"
	else
		elogcmd="elog"
	fi

	# If we have same branch and the tree we can do --depth 1 clone
	# which outputs into really smaller data transfers.
	# Sadly we can do shallow copy for now because quite a few packages need .git
	# folder.
	#[[ ${EGIT_COMMIT} = ${EGIT_BRANCH} ]] && \
	#	EGIT_FETCH_CMD="${EGIT_FETCH_CMD} --depth 1"
	if [[ ! -z ${EGIT_TREE} ]] ; then
		EGIT_COMMIT=${EGIT_TREE}
		ewarn "QA: Usage of deprecated EGIT_TREE variable detected."
		ewarn "QA: Use EGIT_COMMIT variable instead."
	fi

	# EGIT_REPO_URI is empty.
	[[ -z ${EGIT_REPO_URI} ]] && die "${EGIT}: EGIT_REPO_URI is empty."

	# check for the protocol or pull from a local repo.
	if [[ -z ${EGIT_REPO_URI%%:*} ]] ; then
		case ${EGIT_REPO_URI%%:*} in
			git*|http|https|rsync|ssh) ;;
			*) die "${EGIT}: protocol for fetch from "${EGIT_REPO_URI%:*}" is not yet implemented in eclass." ;;
		esac
	fi

	# initial clone, we have to create master git storage directory and play
	# nicely with sandbox
	if [[ ! -d ${EGIT_STORE_DIR} ]] ; then
		debug-print "${FUNCNAME}: initial clone. creating git directory"
		addwrite /
		# TODO(ers): Remove this workaround once we figure out how to make
		# sure the directories are owned by the user instead of by root.
		local old_umask="`umask`"
		umask 002
		mkdir -p "${EGIT_STORE_DIR}" \
			|| die "${EGIT}: can't mkdir ${EGIT_STORE_DIR}."
		umask ${old_umask}
		export SANDBOX_WRITE="${SANDBOX_WRITE%%:/}"
	fi

	cd -P "${EGIT_STORE_DIR}" || die "${EGIT}: can't chdir to ${EGIT_STORE_DIR}"
	EGIT_STORE_DIR=${PWD}

	# allow writing into EGIT_STORE_DIR
	addwrite "${EGIT_STORE_DIR}"

	[[ -z ${EGIT_REPO_URI##*/} ]] && EGIT_REPO_URI="${EGIT_REPO_URI%/}"
	EGIT_CLONE_DIR="${EGIT_PROJECT}"

	debug-print "${FUNCNAME}: EGIT_OPTIONS = \"${EGIT_OPTIONS}\""

	GIT_DIR="${EGIT_STORE_DIR}/${EGIT_CLONE_DIR}"
	# we also have to remove all shallow copied repositories
	# and fetch them again
	if [[ -e "${GIT_DIR}/shallow" ]]; then
		rm -rf "${GIT_DIR}"
		einfo "The ${EGIT_CLONE_DIR} was shallow copy. Refetching."
	fi
	# repack from bare copy to normal one
	if ${EGIT_HAS_SUBMODULES} && [[ -d ${GIT_DIR} && ! -d "${GIT_DIR}/.git/" ]]; then
		rm -rf "${GIT_DIR}"
		einfo "The ${EGIT_CLONE_DIR} was bare copy. Refetching."
	fi
	if ! ${EGIT_HAS_SUBMODULES} && [[ -d ${GIT_DIR} && -d ${GIT_DIR}/.git ]]; then
		rm -rf "${GIT_DIR}"
		einfo "The ${EGIT_CLONE_DIR} was not a bare copy. Refetching."
	fi

	if ${EGIT_HAS_SUBMODULES}; then
		upstream_branch=origin/${EGIT_BRANCH}
	else
		upstream_branch=${EGIT_BRANCH}
		# Note: Normally clones are created using --bare, which does not fetch
		# remote refs and only updates master. This is not okay. --mirror
		# changes that.
		extra_clone_opts=--mirror
	fi

	if [[ ! -d ${GIT_DIR} ]] ; then
		# first clone
		${elogcmd} "GIT NEW clone -->"
		${elogcmd} "   repository: 		${EGIT_REPO_URI}"

		debug-print "${EGIT_FETCH_CMD} ${extra_clone_opts} ${EGIT_OPTIONS} \"${EGIT_REPO_URI}\" ${GIT_DIR}"
		# TODO(ers): Remove this workaround once we figure out how to make
		# sure the directories are owned by the user instead of by root.
		local old_umask="`umask`"
		umask 002
		${EGIT_FETCH_CMD} ${extra_clone_opts} ${EGIT_OPTIONS} "${EGIT_REPO_URI}" ${GIT_DIR} \
			|| die "${EGIT}: can't fetch from ${EGIT_REPO_URI}."

		umask ${old_umask}
		pushd "${GIT_DIR}" &> /dev/null
		cursha1=$(git rev-parse ${upstream_branch})
		${elogcmd} "   at the commit:		${cursha1}"

		git_submodules
		popd &> /dev/null
	elif [[ -n ${EGIT_OFFLINE} ]] ; then
		pushd "${GIT_DIR}" &> /dev/null
		cursha1=$(git rev-parse ${upstream_branch})
		${elogcmd} "GIT offline update -->"
		${elogcmd} "   repository: 		${EGIT_REPO_URI}"
		${elogcmd} "   at the commit:		${cursha1}"
		popd &> /dev/null
	else
		pushd "${GIT_DIR}" &> /dev/null
		# Git urls might change, so unconditionally set it here
		git config remote.origin.url "${EGIT_REPO_URI}"

		# fetch updates
		${elogcmd} "GIT update -->"
		${elogcmd} "   repository: 		${EGIT_REPO_URI}"

		oldsha1=$(git rev-parse ${upstream_branch})

		if ${EGIT_HAS_SUBMODULES}; then
			debug-print "${EGIT_UPDATE_CMD} ${EGIT_OPTIONS}"
			# fix branching
			git checkout ${EGIT_MASTER}
			for x in $(git branch |grep -v "* ${EGIT_MASTER}" |tr '\n' ' '); do
				git branch -D ${x}
			done
			${EGIT_UPDATE_CMD} ${EGIT_OPTIONS} \
				|| die "${EGIT}: can't update from ${EGIT_REPO_URI}."
		elif [[ "${EGIT_COMMIT}" = "${EGIT_BRANCH}" ]]; then
			debug-print "${EGIT_UPDATE_CMD} ${EGIT_OPTIONS} origin ${EGIT_BRANCH}:${EGIT_BRANCH}"
			${EGIT_UPDATE_CMD} ${EGIT_OPTIONS} origin ${EGIT_BRANCH}:${EGIT_BRANCH} \
				|| die "${EGIT}: can't update from ${EGIT_REPO_URI}."
		else
			debug-print "${EGIT_UPDATE_CMD} ${EGIT_OPTIONS} origin"
			${EGIT_UPDATE_CMD} ${EGIT_OPTIONS} origin \
				|| die "${EGIT}: can't update from ${EGIT_REPO_URI}."
		fi

		git_submodules
		cursha1=$(git rev-parse ${upstream_branch})

		# write out message based on the revisions
		if [[ ${oldsha1} != ${cursha1} ]]; then
			${elogcmd} "   updating from commit:	${oldsha1}"
			${elogcmd} "   to commit:		${cursha1}"
		else
			${elogcmd} "   at the commit: 		${cursha1}"
			# @ECLASS_VARIABLE: LIVE_FAIL_FETCH_IF_REPO_NOT_UPDATED
			# @DESCRIPTION:
			# If this variable is set to TRUE in make.conf or somewhere in
			# enviroment the package will fail if there is no update, thus in
			# combination with --keep-going it would lead in not-updating
			# pakcages that are up-to-date.
			# TODO: this can lead to issues if more projects/packages use same repo
			[[ ${LIVE_FAIL_FETCH_IF_REPO_NOT_UPDATED} = true ]] && \
				debug-print "${FUNCNAME}: Repository \"${EGIT_REPO_URI}\" is up-to-date. Skipping." && \
				die "${EGIT}: Repository \"${EGIT_REPO_URI}\" is up-to-date. Skipping."
		fi
		${EGIT_DIFFSTAT_CMD} ${oldsha1}..${upstream_branch}
		popd &> /dev/null
	fi

	pushd "${GIT_DIR}" &> /dev/null
	if ${EGIT_REPACK} || ${EGIT_PRUNE} ; then
		ebegin "Garbage collecting the repository"
		git gc $(${EGIT_PRUNE} && echo '--prune')
		eend $?
	fi
	popd &> /dev/null

	# export the git version
	export EGIT_VERSION="${cursha1}"

	# log the repo state
	[[ ${EGIT_COMMIT} != ${EGIT_BRANCH} ]] && elog "   commit:			${EGIT_COMMIT}"
	${elogcmd} "   branch: 			${EGIT_BRANCH}"
	${elogcmd} "   storage directory: 	\"${GIT_DIR}\""

	if ${EGIT_HAS_SUBMODULES}; then
		pushd "${GIT_DIR}" &> /dev/null
		debug-print "rsync -rlpgo . \"${S}\""
		time rsync -rlpgo . "${S}"
		popd &> /dev/null
	else
		unset GIT_DIR
		debug-print "git clone -l -s -n \"${EGIT_STORE_DIR}/${EGIT_CLONE_DIR}\" \"${S}\""
		git clone -l -s -n "${EGIT_STORE_DIR}/${EGIT_CLONE_DIR}" "${S}"
	fi

	pushd "${S}" &> /dev/null
	git_branch
	# submodules always reqire net (thanks to branches changing)
	[[ -n ${EGIT_OFFLINE} ]] || git_submodules
	popd &> /dev/null

	echo ">>> Unpacked to ${S}"
}

# @FUNCTION: git_bootstrap
# @DESCRIPTION:
# Runs bootstrap command if EGIT_BOOTSTRAP variable contains some value
# Remember that what ever gets to the EGIT_BOOTSTRAP variable gets evaled by bash.
git_bootstrap() {
	debug-print-function ${FUNCNAME} "$@"

	if [[ -n ${EGIT_BOOTSTRAP} ]] ; then
		pushd "${S}" > /dev/null
		einfo "Starting bootstrap"

		if [[ -f ${EGIT_BOOTSTRAP} ]]; then
			# we have file in the repo which we should execute
			debug-print "$FUNCNAME: bootstraping with file \"${EGIT_BOOTSTRAP}\""

			if [[ -x ${EGIT_BOOTSTRAP} ]]; then
				eval "./${EGIT_BOOTSTRAP}" \
					|| die "${EGIT}: bootstrap script failed"
			else
				eerror "\"${EGIT_BOOTSTRAP}\" is not executable."
				eerror "Report upstream, or bug ebuild maintainer to remove bootstrap command."
				die "${EGIT}: \"${EGIT_BOOTSTRAP}\" is not executable."
			fi
		else
			# we execute some system command
			debug-print "$FUNCNAME: bootstraping with commands \"${EGIT_BOOTSTRAP}\""

			eval "${EGIT_BOOTSTRAP}" \
				|| die "${EGIT}: bootstrap commands failed."

		fi

		einfo "Bootstrap finished"
		popd > /dev/null
	fi
}

# @FUNCTION: git_apply_patches
# @DESCRIPTION:
# Apply patches from EGIT_PATCHES bash array.
# Preffered is using the variable as bash array but for now it allows to write
# it also as normal space separated string list. (This part of code should be
# removed when all ebuilds get converted on bash array).
git_apply_patches() {
	debug-print-function ${FUNCNAME} "$@"

	pushd "${S}" > /dev/null
	if [[ ${#EGIT_PATCHES[@]} -gt 1 ]] ; then
		for i in "${EGIT_PATCHES[@]}"; do
			debug-print "$FUNCNAME: git_autopatch: patching from ${i}"
			epatch "${i}"
		done
	elif [[ ${EGIT_PATCHES} != "" ]]; then
		# no need for loop if space separated string is passed.
		debug-print "$FUNCNAME: git_autopatch: patching from ${EGIT_PATCHES}"
		epatch "${EGIT_PATCHES}"
	fi

	popd > /dev/null
}

# @FUNCTION: git_src_unpack
# @DESCRIPTION:
# src_upack function, calls src_prepare one if EAPI!=2.
git_src_unpack() {
	debug-print-function ${FUNCNAME} "$@"

	git_fetch || die "${EGIT}: unknown problem in git_fetch()."

	has src_prepare ${EXPORTED_FUNCTIONS} || git_src_prepare
}

# @FUNCTION: git_src_prepare
# @DESCRIPTION:
# src_prepare function for git stuff. Patches, bootstrap...
git_src_prepare() {
	debug-print-function ${FUNCNAME} "$@"

	git_apply_patches
	git_bootstrap
}
