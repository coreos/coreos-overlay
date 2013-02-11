# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/tla.eclass,v 1.13 2012/09/16 18:37:44 ulm Exp $

# @DEAD
# To be removed on 2012-10-16.

# Original Author:    Jeffrey Yasskin <jyasskin@mail.utexas.edu>
#
# Originally derived from the cvs eclass.
#
# This eclass provides the generic tla fetching functions.
# to use from an ebuild, set the 'ebuild-configurable settings' below in your
# ebuild before inheriting.  then either leave the default src_unpack or extend
# over tla_src_unpack.

# Most of the time, you will define only $ETLA_VERSION and $ETLA_ARCHIVES in
# your ebuild.

# TODO:
# Make it support particular revisions.

inherit eutils

# Don't download anything other than the tla archive
SRC_URI=""

# You shouldn't change these settings yourself! The ebuild/eclass inheriting
# this eclass will take care of that.

# --- begin ebuild-configurable settings

# tla command to run. Theoretically, substituting any arch derivative should be
# relatively easy.
[ -z "$ETLA_TLA_CMD" ] && ETLA_TLA_CMD="tla"

# tla commands with options
[ -z "$ETLA_GET_CMD" ] && ETLA_GET_CMD="get"
[ -z "$ETLA_UPDATE_CMD" ] && ETLA_UPDATE_CMD="replay"

# Where the tla modules are stored/accessed
[ -z "$ETLA_TOP_DIR" ] && ETLA_TOP_DIR="${DISTDIR}/tla-src"

# Name of tla version in the format
#  user@example.com--archive-name/category--branch--version
# (in other words, an argument to tla get, update, or replay)
[ -z "$ETLA_VERSION" ] && ETLA_VERSION=""

# A space-separated list of significant archive URLs. You should definitely
# include the URL for the archive your version is stored in, and if it refers
# to any other archives, also list them.
[ -z "$ETLA_ARCHIVES" ] && ETLA_ARCHIVES=""

# The location in which to cache the version, relative to $ETLA_TOP_DIR.
[ -z "$ETLA_CACHE_DIR" ] && ETLA_CACHE_DIR="${ETLA_VERSION}"

# ETLA_CLEAN: set this to something to get a clean copy when updating (removes
# the working directory, then uses $ETLA_GET_CMD to re-download it.)

# --- end ebuild-configurable settings ---

# add tla to deps
DEPEND="dev-util/tla"

# registers archives mentioned in $ETLA_ARCHIVES
tla_register_archives() {
	debug-print-function $FUNCNAME $* $ETLA_ARCHIVES

	for archive in $ETLA_ARCHIVES; do
		$ETLA_TLA_CMD register-archive -f $archive || die "Could not register archive $archive"
	done
}

# checks that configuration variables have rational values.
tla_check_vars() {
	[ -z "$ETLA_VERSION" ] && die "ETLA_VERSION must be set by the ebuild. Please fix this ebuild."
	$ETLA_TLA_CMD valid-package-name --archive --vsn $ETLA_VERSION || \
		die "ETLA_VERSION has an invalid format. Please fix this ebuild."
}

# is called from tla_src_unpack
tla_fetch() {

	debug-print-function $FUNCNAME $*

	if [ -n "$ETLA_CLEAN" ]; then
		rm -rf $ETLA_TOP_DIR/$ETLA_CACHE_DIR
	fi

	# create the top dir if needed
	if [ ! -d "$ETLA_TOP_DIR" ]; then
		# note that the addwrite statements in this block are only there to allow creating ETLA_TOP_DIR;
		# we've already allowed writing inside it
		# this is because it's simpler than trying to find out the parent path of the directory, which
		# would need to be the real path and not a symlink for things to work (so we can't just remove
		# the last path element in the string)
		debug-print "$FUNCNAME: checkout mode. creating tla directory"
		addwrite /foobar
		addwrite /
		mkdir -p "$ETLA_TOP_DIR"
		export SANDBOX_WRITE="${SANDBOX_WRITE//:\/foobar:\/}"
	fi

	# in case ETLA_TOP_DIR is a symlink to a dir, get the real dir's path,
	# otherwise addwrite() doesn't work.
	cd -P "$ETLA_TOP_DIR" > /dev/null
	ETLA_TOP_DIR="`/bin/pwd`"

	# disable the sandbox for this dir
	addwrite "$ETLA_TOP_DIR"

	# break $ETLA_VERSION into pieces
	local tla_archive=`$ETLA_TLA_CMD parse-package-name --arch $ETLA_VERSION`
	local tla_version=`$ETLA_TLA_CMD parse-package-name --package-version $ETLA_VERSION`
	#local tla_revision=`$ETLA_TLA_CMD parse-package-name --lvl $ETLA_VERSION`

	# determine checkout or update mode and change to the right directory.
	if [ ! -d "$ETLA_TOP_DIR/$ETLA_CACHE_DIR/{arch}" ]; then
		mode=get
		mkdir -p "$ETLA_TOP_DIR/$ETLA_CACHE_DIR"
		cd "$ETLA_TOP_DIR/$ETLA_CACHE_DIR/.."
		rmdir "`basename "$ETLA_CACHE_DIR"`"
	else
		mode=update
		cd "$ETLA_TOP_DIR/$ETLA_CACHE_DIR"
	fi

	# switch versions automagically if needed
	if [ "$mode" == "update" ]; then
		local oldversion="`$ETLA_TLA_CMD tree-version`"
		if [ "$tla_archive/$tla_version" != "$oldversion" ]; then

			einfo "Changing TLA version from $oldversion to $tla_archive/$tla_version:"
			debug-print "$FUNCNAME: Changing TLA version from $oldversion to $tla_archive/$tla_version:"

			$ETLA_TLA_CMD set-tree-version $tla_archive/$tla_version

		fi
	fi

	# commands to run
	local cmdget="${ETLA_TLA_CMD} ${ETLA_GET_CMD} ${ETLA_VERSION} `basename $ETLA_CACHE_DIR`"
	local cmdupdate="${ETLA_TLA_CMD} ${ETLA_UPDATE_CMD} ${ETLA_VERSION}"

	if [ "${mode}" == "get" ]; then
		einfo "Running $cmdget"
		eval $cmdget || die "tla get command failed"
	elif [ "${mode}" == "update" ]; then
		einfo "Running $cmdupdate"
		eval $cmdupdate || die "tla update command failed"
	fi

}


tla_src_unpack() {

	debug-print-function $FUNCNAME $*

	debug-print "$FUNCNAME: init:
	ETLA_TLA_CMD=$ETLA_TLA_CMD
	ETLA_GET_CMD=$ETLA_GET_CMD
	ETLA_UPDATE_CMD=$ETLA_UPDATE_CMD
	ETLA_TOP_DIR=$ETLA_TOP_DIR
	ETLA_VERSION=$ETLA_VERSION
	ETLA_ARCHIVES=$ETLA_ARCHIVES
	ETLA_CACHE_DIR=$ETLA_CACHE_DIR
	ETLA_CLEAN=$ETLA_CLEAN"

	einfo "Registering Archives ..."
	tla_register_archives

	einfo "Checking that passed-in variables are rational ..."
	tla_check_vars

	einfo "Fetching tla version $ETLA_VERSION into $ETLA_TOP_DIR ..."
	tla_fetch

	einfo "Copying $ETLA_CACHE_DIR from $ETLA_TOP_DIR ..."
	debug-print "Copying $ETLA_CACHE_DIR from $ETLA_TOP_DIR ..."

	# probably redundant, but best to make sure
	# Use ${WORKDIR}/${P} rather than ${S} so user can point ${S} to something inside.
	mkdir -p "${WORKDIR}/${P}"

	eshopts_push -s dotglob	# get any dotfiles too.
	cp -Rf "$ETLA_TOP_DIR/$ETLA_CACHE_DIR"/* "${WORKDIR}/${P}"
	eshopts_pop

	# implement some of base_src_unpack's functionality;
	# note however that base.eclass may not have been inherited!
	#if [ -n "$PATCHES" ]; then
	#	debug-print "$FUNCNAME: PATCHES=$PATCHES, S=$S, autopatching"
	#	cd "$S"
	#	for x in $PATCHES; do
	#		debug-print "patching from $x"
	#		patch -p0 < "$x"
	#	done
	#	# make sure we don't try to apply patches more than once, since
	#	# tla_src_unpack may be called several times
	#	export PATCHES=""
	#fi

	einfo "Version ${ETLA_VERSION} is now in ${WORKDIR}/${P}"
}

EXPORT_FUNCTIONS src_unpack
