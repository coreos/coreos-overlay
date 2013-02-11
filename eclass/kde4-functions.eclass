# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/kde4-functions.eclass,v 1.62 2012/09/27 16:35:41 axs Exp $

inherit versionator

# @ECLASS: kde4-functions.eclass
# @MAINTAINER:
# kde@gentoo.org
# @BLURB: Common ebuild functions for KDE 4 packages
# @DESCRIPTION:
# This eclass contains all functions shared by the different eclasses,
# for KDE 4 ebuilds.

# @ECLASS-VARIABLE: EAPI
# @DESCRIPTION:
# Currently kde4 eclasses support EAPI 3 and 4.
case ${EAPI:-0} in
	3|4|5) : ;;
	*) die "EAPI=${EAPI} is not supported" ;;
esac

# @ECLASS-VARIABLE: KDE_OVERRIDE_MINIMAL
# @DESCRIPTION:
# For use only in very few well-defined cases; normally it should be unset.
# If this variable is set, all calls to add_kdebase_dep return a dependency on
# at least this version, independent of the version of the package itself.
# If you know exactly that one specific NEW KDE component builds and runs fine
# with all the rest of KDE at an OLDER version, you can set this old version here.
# Warning- may lead to general instability and kill your pet targh.

# @ECLASS-VARIABLE: KDEBASE
# @DESCRIPTION:
# This gets set to a non-zero value when a package is considered a kde or
# kdevelop ebuild.
if [[ ${CATEGORY} = kde-base ]]; then
	debug-print "${ECLASS}: KDEBASE ebuild recognized"
	KDEBASE=kde-base
elif [[ ${KMNAME-${PN}} = kdevelop ]]; then
	debug-print "${ECLASS}: KDEVELOP ebuild recognized"
	KDEBASE=kdevelop
fi

# determine the build type
if [[ ${PV} = *9999* ]]; then
	KDE_BUILD_TYPE="live"
else
	KDE_BUILD_TYPE="release"
fi
export KDE_BUILD_TYPE

# Set reponame and SCM for modules that have fully migrated to git
# (hack - it's here because it needs to be before SCM inherits from kde4-base)
if [[ ${KDE_BUILD_TYPE} == live ]]; then
	case "${KMNAME}" in
		kdebase-workspace)
			KDE_SCM="git"
			EGIT_REPONAME=${EGIT_REPONAME:=kde-workspace}
		;;
		kdebase-runtime)
			KDE_SCM="git"
			EGIT_REPONAME=${EGIT_REPONAME:=kde-runtime}
		;;
		kdebase-apps)
			KDE_SCM="git"
			EGIT_REPONAME=${EGIT_REPONAME:=kde-baseapps}
		;;
		kde-workspace|kde-runtime|kde-baseapps)
			KDE_SCM="git"
		;;
	esac
fi

# @ECLASS-VARIABLE: KDE_SCM
# @DESCRIPTION:
# If this is a live package which scm does it use
# Everything else uses svn by default
KDE_SCM="${KDE_SCM:-svn}"
case ${KDE_SCM} in
	svn|git) ;;
	*) die "KDE_SCM: ${KDE_SCM} is not supported" ;;
esac

# @ECLASS-VARIABLE: KDE_LINGUAS
# @DESCRIPTION:
# This is a whitespace-separated list of translations this ebuild supports.
# These translations are automatically added to IUSE. Therefore ebuilds must set
# this variable before inheriting any eclasses. To enable only selected
# translations, ebuilds must call enable_selected_linguas(). kde4-{base,meta}.eclass does
# this for you.
#
# Example: KDE_LINGUAS="de en_GB nl"
if [[ ${KDE_BUILD_TYPE} != live || -n ${KDE_LINGUAS_LIVE_OVERRIDE} ]]; then
	for _lingua in ${KDE_LINGUAS}; do
		IUSE="${IUSE} linguas_${_lingua}"
	done
fi

# @FUNCTION: buildsycoca
# @DESCRIPTION:
# Function to rebuild the KDE System Configuration Cache.
# All KDE ebuilds should run this in pkg_postinst and pkg_postrm.
buildsycoca() {
	debug-print-function ${FUNCNAME} "$@"

	# We no longer need to run kbuildsycoca4, as kded does that automatically, as needed

	# fix permission for some directories
	for x in usr/share/{config,kde4}; do
		DIRS=${EROOT}usr
		[[ -d "${EROOT}${x}" ]] || break # nothing to do if directory does not exist
		# fixes Bug 318237
		if use userland_BSD ; then
			[[ $(stat -f %p "${EROOT}${x}") != 40755 ]]
			local stat_rtn="$?"
		else
			[[ $(stat --format=%a "${EROOT}${x}") != 755 ]]
			local stat_rtn=$?
		fi
		if [[ $stat_rtn != 1 ]] ; then
			ewarn "QA Notice:"
			ewarn "Package ${PN} is breaking ${EROOT}${x} permissions."
			ewarn "Please report this issue to gentoo bugzilla."
			einfo "Permissions will get adjusted automatically now."
			find "${EROOT}${x}" -type d -print0 | xargs -0 chmod 755
		fi
	done
}

# @FUNCTION: comment_all_add_subdirectory
# @USAGE: [list of directory names]
# @DESCRIPTION:
# Recursively comment all add_subdirectory instructions in listed directories,
# except those in cmake/.
comment_all_add_subdirectory() {
	find "$@" -name CMakeLists.txt -print0 | grep -vFzZ "./cmake" | \
		xargs -0 sed -i \
			-e '/^[[:space:]]*add_subdirectory/s/^/#DONOTCOMPILE /' \
			-e '/^[[:space:]]*ADD_SUBDIRECTORY/s/^/#DONOTCOMPILE /' \
			-e '/^[[:space:]]*macro_optional_add_subdirectory/s/^/#DONOTCOMPILE /' \
			-e '/^[[:space:]]*MACRO_OPTIONAL_ADD_SUBDIRECTORY/s/^/#DONOTCOMPILE /' \
			|| die "${LINENO}: Initial sed died"
}

# @FUNCTION: enable_selected_linguas
# @DESCRIPTION:
# Enable translations based on LINGUAS settings and translations supported by
# the package (see KDE_LINGUAS). By default, translations are found in "${S}"/po
# but this default can be overridden by defining KDE_LINGUAS_DIR.
enable_selected_linguas() {
	debug-print-function ${FUNCNAME} "$@"

	local x

	# if there is no linguas defined we enable everything
	if ! $(env | grep -q "^LINGUAS="); then
		return 0
	fi

	# @ECLASS-VARIABLE: KDE_LINGUAS_DIR
	# @DESCRIPTION:
	# Specified folder where application translations are located.
	# Can be defined as array of folders where translations are located.
	# Note that space separated list of dirs is not supported.
	# Default value is set to "po".
	if [[ "$(declare -p KDE_LINGUAS_DIR 2>/dev/null 2>&1)" == "declare -a"* ]]; then
		debug-print "$FUNCNAME: we have these subfolders defined: ${KDE_LINGUAS_DIR}"
		for x in ${KDE_LINGUAS_DIR[@]}; do
			_enable_selected_linguas_dir ${x}
		done
	else
		KDE_LINGUAS_DIR=${KDE_LINGUAS_DIR:="po"}
		_enable_selected_linguas_dir ${KDE_LINGUAS_DIR}
	fi
}

# @FUNCTION: enable_selected_doc_linguas
# @DESCRIPTION:
# Enable only selected linguas enabled doc folders.
enable_selected_doc_linguas() {
	debug-print-function ${FUNCNAME} "$@"

	# @ECLASS-VARIABLE: KDE_DOC_DIRS
	# @DESCRIPTION:
	# Variable specifying whitespace separated patterns for documentation locations.
	# Default is "doc/%lingua"
	KDE_DOC_DIRS=${KDE_DOC_DIRS:='doc/%lingua'}
	local linguas
	for pattern in ${KDE_DOC_DIRS}; do

		local handbookdir=`dirname ${pattern}`
		local translationdir=`basename ${pattern}`
		# Do filename pattern supplied, treat as directory
		[[ ${handbookdir} = '.' ]] && handbookdir=${translationdir} && translationdir=
		[[ -d ${handbookdir} ]] || die 'wrong doc dir specified'

		if ! use handbook; then
			# Disable whole directory
			sed -e "/add_subdirectory[[:space:]]*([[:space:]]*${handbookdir}[[:space:]]*)/s/^/#DONOTCOMPILE /" \
				-e "/ADD_SUBDIRECTORY[[:space:]]*([[:space:]]*${handbookdir}[[:space:]]*)/s/^/#DONOTCOMPILE /" \
				-i CMakeLists.txt || die 'failed to comment out all handbooks'
		else
			# if there is no linguas defined we enable everything (i.e. comment out nothing)
			if ! $(env | grep -q "^LINGUAS="); then
				return 0
			fi

			# Disable subdirectories recursively
			comment_all_add_subdirectory "${handbookdir}"
			# Add requested translations
			local lingua
			for lingua in en ${KDE_LINGUAS}; do
				if [[ ${lingua} = en ]] || use linguas_${lingua}; then
					if [[ -d ${handbookdir}/${translationdir//%lingua/${lingua}} ]]; then
						sed -e "/add_subdirectory[[:space:]]*([[:space:]]*${translationdir//%lingua/${lingua}}/s/^#DONOTCOMPILE //" \
							-e "/ADD_SUBDIRECTORY[[:space:]]*([[:space:]]*${translationdir//%lingua/${lingua}}/s/^#DONOTCOMPILE //" \
							-i "${handbookdir}"/CMakeLists.txt && ! has ${lingua} ${linguas} && linguas="${linguas} ${lingua}"
					fi
				fi
			done
		fi

	done
	[[ -n "${linguas}" ]] && einfo "Enabling handbook translations:${linguas}"
}

# @FUNCTION: migrate_store_dir
# @DESCRIPTION:
# Universal store dir migration
# * performs split of kdebase to kdebase-apps when needed
# * moves playground/extragear kde4-base-style to toplevel dir
migrate_store_dir() {
	if [[ ${KDE_SCM} != svn ]]; then
		die "migrate_store_dir() only makes sense for subversion"
	fi

	local cleandir="${ESVN_STORE_DIR}/KDE"

	if [[ -d ${cleandir} ]]; then
		ewarn "'${cleandir}' has been found. Moving contents to new location."
		addwrite "${ESVN_STORE_DIR}"
		# Split kdebase
		local module
		if pushd "${cleandir}"/kdebase/kdebase > /dev/null; then
			for module in `find . -maxdepth 1 -type d -name [a-z0-9]\*`; do
				module="${module#./}"
				mkdir -p "${ESVN_STORE_DIR}/kdebase-${module}" && mv -f "${module}" "${ESVN_STORE_DIR}/kdebase-${module}" || \
					die "Failed to move to '${ESVN_STORE_DIR}/kdebase-${module}'."
			done
			popd > /dev/null
			rm -fr "${cleandir}/kdebase" || \
				die "Failed to remove ${cleandir}/kdebase. You need to remove it manually."
		fi
		# Move the rest
		local pkg
		for pkg in "${cleandir}"/*; do
			mv -f "${pkg}" "${ESVN_STORE_DIR}"/ || eerror "Failed to move '${pkg}'"
		done
		rmdir "${cleandir}" || die "Could not move obsolete KDE store dir.  Please move '${cleandir}' contents to appropriate location (possibly ${ESVN_STORE_DIR}) and manually remove '${cleandir}' in order to continue."
	fi

	if ! has kde4-meta ${INHERITED}; then
		case ${KMNAME} in
			extragear*|playground*)
				local scmlocalpath="${ESVN_STORE_DIR}"/"${KMNAME}"/"${PN}"
				if [[ -d "${scmlocalpath}" ]]; then
					local destdir="${ESVN_STORE_DIR}"/"${ESVN_PROJECT}"/"`basename "${ESVN_REPO_URI}"`"
					ewarn "'${scmlocalpath}' has been found."
					ewarn "Moving contents to new location: ${destdir}"
					addwrite "${ESVN_STORE_DIR}"
					mkdir -p "${ESVN_STORE_DIR}"/"${ESVN_PROJECT}" && mv -f "${scmlocalpath}" "${destdir}" \
						|| die "Failed to move to '${scmlocalpath}'"
					# Try cleaning empty directories
					rmdir "`dirname "${scmlocalpath}"`" 2> /dev/null
				fi
				;;
		esac
	fi
}

# Functions handling KMLOADLIBS and KMSAVELIBS

# @FUNCTION: save_library_dependencies
# @DESCRIPTION:
# Add exporting CMake dependencies for current package
save_library_dependencies() {
	local depsfile="${T}/${PN}"

	ebegin "Saving library dependencies in ${depsfile##*/}"
	echo "EXPORT_LIBRARY_DEPENDENCIES(\"${depsfile}\")" >> "${S}/CMakeLists.txt" || \
		die "Failed to save the library dependencies."
	eend $?
}

# @FUNCTION: install_library_dependencies
# @DESCRIPTION:
# Install generated CMake library dependencies to /var/lib/kde
install_library_dependencies() {
	local depsfile="${T}/${PN}"

	ebegin "Installing library dependencies as ${depsfile##*/}"
	insinto /var/lib/kde
	doins "${depsfile}" || die "Failed to install library dependencies."
	eend $?
}

# @FUNCTION: load_library_dependencies
# @DESCRIPTION:
# Inject specified library dependencies in current package
load_library_dependencies() {
	local pn i depsfile
	ebegin "Injecting library dependencies from '${KMLOADLIBS}'"

	i=0
	for pn in ${KMLOADLIBS} ; do
		((i++))
		depsfile="${EPREFIX}/var/lib/kde/${pn}"
		[[ -r ${depsfile} ]] || depsfile="${EPREFIX}/var/lib/kde/${pn}:$(get_kde_version)"
		[[ -r ${depsfile} ]] || die "Depsfile '${depsfile}' not accessible. You probably need to reinstall ${pn}."
		sed -i -e "${i}iINCLUDE(\"${depsfile}\")" "${S}/CMakeLists.txt" || \
			die "Failed to include library dependencies for ${pn}"
	done
	eend $?
}

# @FUNCTION: add_blocker
# @DESCRIPTION:
# Create correct RDEPEND value for blocking correct package.
# Useful for file-collision blocks.
# Parameters are package and version(s) to block.
# add_blocker kdelibs 4.2.4
# If no version is specified, then all versions will be blocked.
# If the version is 0, then no versions will be blocked.
# If a second version ending in ":3.5" is passed, then the version listed for
# that slot will be blocked as well.
#
# Examples:
#    # Block all versions of kdelibs
#    add_blocker kdelibs
#
#    # Block all versions of kdelibs older than 4.3.50
#    add_blocker kdelibs 4.3.50
#
#    # Block kdelibs 3.5.10 and older, but not any version of
#    # kdelibs from KDE 4
#    add_blocker kdelibs 0 3.5.10:3.5
add_blocker() {
	debug-print-function ${FUNCNAME} "$@"

	[[ -z ${1} ]] && die "Missing parameter"
	local pkg=kde-base/$1 atom old_ver="unset" use
	if [[ $pkg == *\[*\] ]]; then
		use=${pkg/#*\[/[}
		pkg=${pkg%\[*\]}
	fi

	[[ "$3" == *:3.5 ]] && old_ver=${3%:3.5}

	# If the version passed is "0", do nothing
	if [[ ${2} != 0 ]]; then
		# If no version was passed, block all versions in this slot
		if [[ -z ${2} ]]; then
			atom=${pkg}
		# If the version passed begins with a "<", then use "<" instead of "<="
		elif [[ ${2::1} == "<" ]]; then
			# this also removes the first character of the version, which is a "<"
			atom="<${pkg}-${2:1}"
		else
			atom="<=${pkg}-${2}"
		fi
		RDEPEND+=" !${atom}:4${use}"
	fi

	# Do the same thing as above for :3.5, except that we don't want any
	# output if no parameter was passed.
	if [[ ${old_ver} != "unset" ]]; then
		if [[ -z ${old_ver} ]]; then
			atom=${pkg}
		elif [[ ${old_ver::1} == "<" ]]; then
			atom="<${pkg}-${old_ver:1}"
		else
			atom="<=${pkg}-${old_ver}"
		fi
		RDEPEND+=" !${atom}:3.5${use}"
	fi
}

# @FUNCTION: add_kdebase_dep
# @DESCRIPTION:
# Create proper dependency for kde-base/ dependencies.
# This takes 1 to 3 arguments. The first being the package name, the optional
# second is additional USE flags to append, and the optional third is the
# version to use instead of the automatic version (use sparingly).
# The output of this should be added directly to DEPEND/RDEPEND, and may be
# wrapped in a USE conditional (but not an || conditional without an extra set
# of parentheses).
add_kdebase_dep() {
	debug-print-function ${FUNCNAME} "$@"

	local ver

	if [[ -n ${3} ]]; then
		ver=${3}
	elif [[ -n ${KDE_OVERRIDE_MINIMAL} ]]; then
		ver=${KDE_OVERRIDE_MINIMAL}
	elif [[ ${KDEBASE} != kde-base ]]; then
		ver=${KDE_MINIMAL}
	# if building stable-live version depend just on the raw KDE version
	# to allow merging packages against more stable basic stuff
	elif [[ ${PV} == *.9999 ]]; then
		ver=$(get_kde_version)
	else
		ver=${PV}
	fi

	[[ -z ${1} ]] && die "Missing parameter"

	echo " >=kde-base/${1}-${ver}:4[aqua=${2:+,${2}}]"
}

# local function to enable specified translations for specified directory
# used from kde4-functions_enable_selected_linguas function
_enable_selected_linguas_dir() {
	local lingua linguas sr_mess wp
	local dir=${1}

	[[ -d  ${dir} ]] || die "linguas dir \"${dir}\" does not exist"
	comment_all_add_subdirectory "${dir}"
	pushd "${dir}" > /dev/null

	# fix all various crazy sr@Latn variations
	# this part is only ease for ebuilds, so there wont be any die when this
	# fail at any point
	sr_mess="sr@latn sr@latin sr@Latin"
	for wp in ${sr_mess}; do
		[[ -e ${wp}.po ]] && mv "${wp}.po" "sr@Latn.po"
		if [[ -d ${wp} ]]; then
			# move dir and fix cmakelists
			mv "${wp}" "sr@Latn"
			sed -i \
				-e "s:${wp}:sr@Latn:g" \
				CMakeLists.txt
		fi
	done

	for lingua in ${KDE_LINGUAS}; do
		if [[ -e ${lingua}.po ]]; then
			mv "${lingua}.po" "${lingua}.po.old"
		fi
	done

	for lingua in ${KDE_LINGUAS}; do
		if use linguas_${lingua} ; then
			if [[ -d ${lingua} ]]; then
				linguas="${linguas} ${lingua}"
				sed -e "/add_subdirectory([[:space:]]*${lingua}[[:space:]]*)[[:space:]]*$/ s/^#DONOTCOMPILE //" \
					-e "/ADD_SUBDIRECTORY([[:space:]]*${lingua}[[:space:]]*)[[:space:]]*$/ s/^#DONOTCOMPILE //" \
					-i CMakeLists.txt || die "Sed to uncomment linguas_${lingua} failed."
			fi
			if [[ -e ${lingua}.po.old ]]; then
				linguas="${linguas} ${lingua}"
				mv "${lingua}.po.old" "${lingua}.po"
			fi
		fi
	done
	[[ -n ${linguas} ]] && echo ">>> Enabling languages: ${linguas}"

	popd > /dev/null
}

# @FUNCTION: get_kde_version
# @DESCRIPTION:
# Translates an ebuild version into a major.minor KDE SC
# release version. If no version is specified, ${PV} is used.
get_kde_version() {
	local ver=${1:-${PV}}
	local major=$(get_major_version ${ver})
	local minor=$(get_version_component_range 2 ${ver})
	local micro=$(get_version_component_range 3 ${ver})
	if [[ ${ver} == 9999 ]]; then
		echo live
	else
		(( micro < 50 )) && echo ${major}.${minor} || echo ${major}.$((minor + 1))
	fi
}
