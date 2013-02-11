# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/games.eclass,v 1.153 2012/09/27 16:35:41 axs Exp $

# devlist: games@gentoo.org
#
# This is the games eclass for standardizing the install of games ...
# you better have a *good* reason why you're *not* using games.eclass
# in a games-* ebuild

if [[ ${___ECLASS_ONCE_GAMES} != "recur -_+^+_- spank" ]] ; then
___ECLASS_ONCE_GAMES="recur -_+^+_- spank"

inherit base multilib toolchain-funcs eutils user

case ${EAPI:-0} in
	0|1) EXPORT_FUNCTIONS pkg_setup src_compile pkg_preinst pkg_postinst ;;
	2|3|4|5) EXPORT_FUNCTIONS pkg_setup src_configure src_compile pkg_preinst pkg_postinst ;;
	*) die "no support for EAPI=${EAPI} yet" ;;
esac

export GAMES_PREFIX=${GAMES_PREFIX:-/usr/games}
export GAMES_PREFIX_OPT=${GAMES_PREFIX_OPT:-/opt}
export GAMES_DATADIR=${GAMES_DATADIR:-/usr/share/games}
export GAMES_DATADIR_BASE=${GAMES_DATADIR_BASE:-/usr/share} # some packages auto append 'games'
export GAMES_SYSCONFDIR=${GAMES_SYSCONFDIR:-/etc/games}
export GAMES_STATEDIR=${GAMES_STATEDIR:-/var/games}
export GAMES_LOGDIR=${GAMES_LOGDIR:-/var/log/games}
export GAMES_BINDIR=${GAMES_BINDIR:-${GAMES_PREFIX}/bin}
export GAMES_ENVD="90games"
# if you want to use a different user/group than games.games,
# just add these two variables to your environment (aka /etc/profile)
export GAMES_USER=${GAMES_USER:-root}
export GAMES_USER_DED=${GAMES_USER_DED:-games}
export GAMES_GROUP=${GAMES_GROUP:-games}

games_get_libdir() {
	echo ${GAMES_PREFIX}/$(get_libdir)
}

egamesconf() {
	econf \
		--prefix="${GAMES_PREFIX}" \
		--libdir="$(games_get_libdir)" \
		--datadir="${GAMES_DATADIR}" \
		--sysconfdir="${GAMES_SYSCONFDIR}" \
		--localstatedir="${GAMES_STATEDIR}" \
		"$@"
}

gameswrapper() {
	# dont want to pollute calling env
	(
		into "${GAMES_PREFIX}"
		cmd=$1
		shift
		${cmd} "$@"
	)
}

dogamesbin() { gameswrapper ${FUNCNAME/games} "$@"; }
dogamessbin() { gameswrapper ${FUNCNAME/games} "$@"; }
dogameslib() { gameswrapper ${FUNCNAME/games} "$@"; }
dogameslib.a() { gameswrapper ${FUNCNAME/games} "$@"; }
dogameslib.so() { gameswrapper ${FUNCNAME/games} "$@"; }
newgamesbin() { gameswrapper ${FUNCNAME/games} "$@"; }
newgamessbin() { gameswrapper ${FUNCNAME/games} "$@"; }

games_make_wrapper() { gameswrapper ${FUNCNAME/games_} "$@"; }

gamesowners() { chown ${GAMES_USER}:${GAMES_GROUP} "$@"; }
gamesperms() { chmod u+rw,g+r-w,o-rwx "$@"; }
prepgamesdirs() {
	local dir f mode
	for dir in \
		"${GAMES_PREFIX}" "${GAMES_PREFIX_OPT}" "${GAMES_DATADIR}" \
		"${GAMES_SYSCONFDIR}" "${GAMES_STATEDIR}" "$(games_get_libdir)" \
		"${GAMES_BINDIR}" "$@"
	do
		[[ ! -d ${D}/${dir} ]] && continue
		(
			gamesowners -R "${D}/${dir}"
			find "${D}/${dir}" -type d -print0 | xargs -0 chmod 750
			mode=o-rwx,g+r,g-w
			[[ ${dir} = ${GAMES_STATEDIR} ]] && mode=o-rwx,g+r
			find "${D}/${dir}" -type f -print0 | xargs -0 chmod $mode

			# common trees should not be games owned #264872
			if [[ ${dir} == "${GAMES_PREFIX_OPT}" ]] ; then
				fowners root:root "${dir}"
				fperms 755 "${dir}"
				for d in $(get_libdir) bin ; do
					# check if dirs exist to avoid "nonfatal" option
					if [[ -e ${D}/${dir}/${d} ]] ; then
						fowners root:root "${dir}/${d}"
						fperms 755 "${dir}/${d}"
					fi
				done
			fi
		) &>/dev/null

		f=$(find "${D}/${dir}" -perm +4000 -a -uid 0 2>/dev/null)
		if [[ -n ${f} ]] ; then
			eerror "A game was detected that is setuid root!"
			eerror "${f}"
			die "refusing to merge a setuid root game"
		fi
	done
	[[ -d ${D}/${GAMES_BINDIR} ]] || return 0
	find "${D}/${GAMES_BINDIR}" -maxdepth 1 -type f -exec chmod 750 '{}' \;
}

gamesenv() {
	local d libdirs

	for d in $(get_all_libdirs) ; do
		libdirs="${libdirs}:${GAMES_PREFIX}/${d}"
	done

	# Wish we could use doevnd here, but we dont want the env
	# file to be tracked in the CONTENTS of every game
	cat <<-EOF > "${ROOT}"/etc/env.d/${GAMES_ENVD}
	LDPATH="${libdirs:1}"
	PATH="${GAMES_BINDIR}"
	EOF
}

games_pkg_setup() {
	tc-export CC CXX LD AR RANLIB

	enewgroup "${GAMES_GROUP}" 35
	[[ ${GAMES_USER} != "root" ]] \
		&& enewuser "${GAMES_USER}" 35 -1 "${GAMES_PREFIX}" "${GAMES_GROUP}"
	[[ ${GAMES_USER_DED} != "root" ]] \
		&& enewuser "${GAMES_USER_DED}" 36 /bin/bash "${GAMES_PREFIX}" "${GAMES_GROUP}"

	# Dear portage team, we are so sorry.  Lots of love, games team.
	# See Bug #61680
	[[ ${USERLAND} != "GNU" ]] && return 0
	[[ $(egetshell "${GAMES_USER_DED}") == "/bin/false" ]] \
		&& usermod -s /bin/bash "${GAMES_USER_DED}"
}

games_src_configure() {
	[[ -x ./configure ]] && egamesconf
}

games_src_compile() {
	case ${EAPI:-0} in
		0|1) games_src_configure ;;
	esac
	base_src_make
}

games_pkg_preinst() {
	local f

	while read f ; do
		if [[ -e ${ROOT}/${GAMES_STATEDIR}/${f} ]] ; then
			cp -p \
				"${ROOT}/${GAMES_STATEDIR}/${f}" \
				"${D}/${GAMES_STATEDIR}/${f}" \
				|| die "cp failed"
			# make the date match the rest of the install
			touch "${D}/${GAMES_STATEDIR}/${f}"
		fi
	done < <(find "${D}/${GAMES_STATEDIR}" -type f -printf '%P\n' 2>/dev/null)
}

# pkg_postinst function ... create env.d entry and warn about games group
games_pkg_postinst() {
	gamesenv
	if [[ -z "${GAMES_SHOW_WARNING}" ]] ; then
		ewarn "Remember, in order to play games, you have to"
		ewarn "be in the '${GAMES_GROUP}' group."
		echo
		case ${CHOST} in
			*-darwin*) ewarn "Just run 'niutil -appendprop / /groups/games users <USER>'";;
			*-freebsd*|*-dragonfly*) ewarn "Just run 'pw groupmod ${GAMES_GROUP} -m <USER>'";;
			*) ewarn "Just run 'gpasswd -a <USER> ${GAMES_GROUP}', then have <USER> re-login.";;
		esac
		echo
		einfo "For more info about Gentoo gaming in general, see our website:"
		einfo "   http://games.gentoo.org/"
		echo
	fi
}

# Unpack .uz2 files for UT2003/UT2004
# $1: directory or file to unpack
games_ut_unpack() {
	local ut_unpack="$1"
	local f=

	if [[ -z ${ut_unpack} ]] ; then
		die "You must provide an argument to games_ut_unpack"
	fi
	if [[ -f ${ut_unpack} ]] ; then
		uz2unpack "${ut_unpack}" "${ut_unpack%.uz2}" \
			|| die "uncompressing file ${ut_unpack}"
	fi
	if [[ -d ${ut_unpack} ]] ; then
		while read f ; do
			uz2unpack "${ut_unpack}/${f}" "${ut_unpack}/${f%.uz2}" \
				|| die "uncompressing file ${f}"
			rm -f "${ut_unpack}/${f}" || die "deleting compressed file ${f}"
		done < <(find "${ut_unpack}" -maxdepth 1 -name '*.uz2' -printf '%f\n' 2>/dev/null)
	fi
}

# Unpacks .umod/.ut2mod/.ut4mod files for UT/UT2003/UT2004
# Usage: games_umod_unpack $1
# oh, and don't forget to set 'dir' and 'Ddir'
games_umod_unpack() {
	local umod=$1
	mkdir -p "${Ddir}"/System
	cp "${dir}"/System/{ucc-bin,{Manifest,Def{ault,User}}.ini,{Engine,Core,zlib,ogg,vorbis}.so,{Engine,Core}.int} "${Ddir}"/System
	cd "${Ddir}"/System
	UT_DATA_PATH=${Ddir}/System ./ucc-bin umodunpack -x "${S}/${umod}" -nohomedir &> /dev/null \
		|| die "uncompressing file ${umod}"
	rm -f "${Ddir}"/System/{ucc-bin,{Manifest,Def{ault,User},User,UT200{3,4}}.ini,{Engine,Core,zlib,ogg,vorbis}.so,{Engine,Core}.int,ucc.log} &>/dev/null \
		|| die "Removing temporary files"
}

fi
