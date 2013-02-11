# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/vdr-plugin.eclass,v 1.90 2012/09/27 16:35:42 axs Exp $

# @ECLASS: vdr-plugin.eclass
# @MAINTAINER:
# vdr@gentoo.org
# @BLURB: common vdr plugin ebuild functions
# @DESCRIPTION:
# Eclass for easing maitenance of vdr plugin ebuilds

# Authors:
# Matthias Schwarzott <zzam@gentoo.org>
# Joerg Bornkessel <hd_brummy@gentoo.org>
# Christian Ruppert <idl0r@gentoo.org>

# Plugin config file installation:
#
# A plugin config file can be specified through the $VDR_CONFD_FILE variable, it
# defaults to ${FILESDIR}/confd. Each config file will be installed as e.g.
# ${D}/etc/conf.d/vdr.${VDRPLUGIN}

# Installation of rc-addon files:
# NOTE: rc-addon files must be valid shell scripts!
#
# Installing rc-addon files is basically the same as for plugin config files
# (see above), it's just using the $VDR_RCADDON_FILE variable instead.
# The default value when $VDR_RCADDON_FILE is undefined is:
# ${FILESDIR}/rc-addon.sh and will be installed as
# ${VDR_RC_DIR}/plugin-${VDRPLUGIN}.sh
#
# The rc-addon files will be sourced by the startscript when the specific plugin
# has been enabled.
# rc-addon files may be used to prepare everything that is necessary for the
# plugin start/stop, like passing extra command line options and so on.

# Applying your own local/user patches:
# This is done by using the epatch_user() function of the eutils.eclass.
# Simply put your patches into one of these directories:
# /etc/portage/patches/<CATEGORY>/<PF|P|PN>/
# Quote: where the first of these three directories to exist will be the one to
# use, ignoring any more general directories which might exist as well.
#
# For more details about it please take a look at the eutils.class.

inherit base multilib eutils flag-o-matic

if ! has "${EAPI:-0}" 0 1 2 3 4 5; then
	die "API of vdr-plugin.eclass in EAPI=\"${EAPI}\" not established"
fi

IUSE=""

# Name of the plugin stripped from all vdrplugin-, vdr- and -cvs pre- and postfixes
VDRPLUGIN="${PN/#vdrplugin-/}"
VDRPLUGIN="${VDRPLUGIN/#vdr-/}"
VDRPLUGIN="${VDRPLUGIN/%-cvs/}"

DESCRIPTION="vdr Plugin: ${VDRPLUGIN} (based on vdr-plugin.eclass)"

# works in most cases
S="${WORKDIR}/${VDRPLUGIN}-${PV}"

# depend on headers for DVB-driver
COMMON_DEPEND=">=media-tv/gentoo-vdr-scripts-0.4.2"

DEPEND="${COMMON_DEPEND}
	virtual/linuxtv-dvb-headers"
RDEPEND="${COMMON_DEPEND}
	>=app-admin/eselect-vdr-0.0.2"

# this is a hack for ebuilds like vdr-xineliboutput that want to
# conditionally install a vdr-plugin
if [[ "${GENTOO_VDR_CONDITIONAL:-no}" = "yes" ]]; then
	# make DEPEND conditional
	IUSE="${IUSE} vdr"
	DEPEND="vdr? ( ${DEPEND} )"
	RDEPEND="vdr? ( ${RDEPEND} )"
fi

# New method of storing plugindb
#   Called from src_install
#   file maintained by normal portage-methods
create_plugindb_file() {
	local NEW_VDRPLUGINDB_DIR=/usr/share/vdr/vdrplugin-rebuild/
	local DB_FILE="${NEW_VDRPLUGINDB_DIR}/${CATEGORY}-${PF}"
	insinto "${NEW_VDRPLUGINDB_DIR}"

#	BUG: portage-2.1.4_rc9 will delete the EBUILD= line, so we cannot use this code.
#	cat <<-EOT > "${D}/${DB_FILE}"
#		VDRPLUGIN_DB=1
#		CREATOR=ECLASS
#		EBUILD=${CATEGORY}/${PN}
#		EBUILD_V=${PVR}
#	EOT
	{
		echo "VDRPLUGIN_DB=1"
		echo "CREATOR=ECLASS"
		echo "EBUILD=${CATEGORY}/${PN}"
		echo "EBUILD_V=${PVR}"
		echo "PLUGINS=\"$@\""
	} > "${D}/${DB_FILE}"
}

# Delete files created outside of vdr-plugin.eclass
#   vdrplugin-rebuild.ebuild converted plugindb and files are
#   not deleted by portage itself - should only be needed as
#   long as not every system has switched over to
#   vdrplugin-rebuild-0.2 / gentoo-vdr-scripts-0.4.2
delete_orphan_plugindb_file() {
	#elog Testing for orphaned plugindb file
	local NEW_VDRPLUGINDB_DIR=/usr/share/vdr/vdrplugin-rebuild/
	local DB_FILE="${ROOT}/${NEW_VDRPLUGINDB_DIR}/${CATEGORY}-${PF}"

	# file exists
	[[ -f ${DB_FILE} ]] || return

	# will portage handle the file itself
	if grep -q CREATOR=ECLASS "${DB_FILE}"; then
		#elog file owned by eclass - don't touch it
		return
	fi

	elog "Removing orphaned plugindb-file."
	elog "\t#rm ${DB_FILE}"
	rm "${DB_FILE}"
}


create_header_checksum_file()
{
	# Danger: Not using $ROOT here, as compile will also not use it !!!
	# If vdr in $ROOT and / differ, plugins will not run anyway

	local CHKSUM="header-md5-vdr"

	if [[ -f ${VDR_CHECKSUM_DIR}/header-md5-vdr ]]; then
		cp "${VDR_CHECKSUM_DIR}/header-md5-vdr" "${CHKSUM}"
	elif type -p md5sum >/dev/null 2>&1; then
		(
			cd "${VDR_INCLUDE_DIR}"
			md5sum *.h libsi/*.h|LC_ALL=C sort --key=2
		) > "${CHKSUM}"
	else
		die "Could not create md5 checksum of headers"
	fi

	insinto "${VDR_CHECKSUM_DIR}"
	local p_name
	for p_name; do
		newins "${CHKSUM}" "header-md5-${p_name}"
	done
}

fix_vdr_libsi_include()
{
	#einfo "Fixing include of libsi-headers"
	local f
	for f; do
		sed -i "${f}" \
			-e '/#include/s:"\(.*libsi.*\)":<\1>:' \
			-e '/#include/s:<.*\(libsi/.*\)>:<vdr/\1>:'
	done
}

vdr_patchmakefile() {
	einfo "Patching Makefile"
	[[ -e Makefile ]] || die "Makefile of plugin can not be found!"
	cp Makefile "${WORKDIR}"/Makefile.before

	# plugin makefiles use VDRDIR in strange ways
	# assumptions:
	#   1. $(VDRDIR) contains Make.config
	#   2. $(VDRDIR) contains config.h
	#   3. $(VDRDIR)/include/vdr contains the headers
	#   4. $(VDRDIR) contains main vdr Makefile
	#   5. $(VDRDIR)/locale exists
	#   6. $(VDRDIR) allows to access vdr source files
	#
	# We only have one directory (for now /usr/include/vdr),
	# that contains vdr-headers and Make.config.
	# To satisfy 1-3 we do this:
	#   Set VDRDIR=/usr/include/vdr
	#   Set VDRINCDIR=/usr/include
	#   Change $(VDRDIR)/include to $(VDRINCDIR)

	sed -i Makefile \
		-e "s:^VDRDIR.*$:VDRDIR = ${VDR_INCLUDE_DIR}:" \
		-e "/^VDRDIR/a VDRINCDIR = ${VDR_INCLUDE_DIR%/vdr}" \
		-e '/VDRINCDIR.*=/!s:$(VDRDIR)/include:$(VDRINCDIR):' \
		\
		-e 's:-I$(DVBDIR)/include::' \
		-e 's:-I$(DVBDIR)::'

	# maybe needed for multiproto:
	#sed -i Makefile \
	#	-e "s:^DVBDIR.*$:DVBDIR = ${DVB_INCLUDE_DIR}:" \
	#	-e 's:-I$(DVBDIR)/include:-I$(DVBDIR):'

	if ! grep -q APIVERSION Makefile; then
		ebegin "  Converting to APIVERSION"
		sed -i Makefile \
			-e 's:^APIVERSION = :APIVERSION ?= :' \
			-e 's:$(LIBDIR)/$@.$(VDRVERSION):$(LIBDIR)/$@.$(APIVERSION):' \
			-e '/VDRVERSION =/a\APIVERSION = $(shell sed -ne '"'"'/define APIVERSION/s/^.*"\\(.*\\)".*$$/\\1/p'"'"' $(VDRDIR)/config.h)'
		eend $?
	fi

	# Correcting Compile-Flags
	# Do not overwrite CXXFLAGS, add LDFLAGS if missing
	sed -i Makefile \
		-e '/^CXXFLAGS[[:space:]]*=/s/=/?=/' \
		-e '/LDFLAGS/!s:-shared:$(LDFLAGS) -shared:'

	# Disabling file stripping, useful for debugging
	sed -i Makefile \
		-e '/@.*strip/d' \
		-e '/strip \$(LIBDIR)\/\$@/d' \
		-e 's/STRIP.*=.*$/STRIP = true/'

	# Use a file instead of a variable as single-stepping via ebuild
	# destroys environment.
	touch "${WORKDIR}"/.vdr-plugin_makefile_patched
}

vdr_has_gettext() {
	has_version ">=media-video/vdr-1.5.7"
}

plugin_has_gettext() {
	[[ -d po ]]
}

vdr_i18n_convert_to_gettext() {
	if has_version ">=media-video/vdr-1.7.22"; then
		local i18n_tool="${ROOT}/usr/share/vdr/bin/i18n-to-gettext"
	else
		local i18n_tool="${ROOT}/usr/share/vdr/bin/i18n-to-gettext.pl"
	fi

	if [[ ${NO_GETTEXT_HACK} == "1" ]]; then
		ewarn "Conversion to gettext disabled in ebuild"
		return 1
	fi

	if [[ ! -x ${i18n_tool} ]]; then
		eerror "Missing ${i18n_tool}"
		eerror "Please re-emerge vdr"
		die "Missing ${i18n_tool}"
	fi

	ebegin "Auto converting translations to gettext"
	# call i18n-to-gettext tool
	# take all texts missing tr call into special file
	"${i18n_tool}" 2>/dev/null \
		|sed -e '/^"/!d' \
			-e '/^""$/d' \
			-e 's/\(.*\)/trNOOP(\1)/' \
		> dummy-translations-trNOOP.c

	# if there were untranslated texts just run it again
	# now the missing calls are listed in
	# dummy-translations-trNOOP.c
	if [[ -s dummy-translations-trNOOP.c ]]; then
		"${i18n_tool}" &>/dev/null
	fi

	# now use the modified Makefile
	if [[ -f Makefile.new ]]; then
		mv Makefile.new Makefile
		eend 0 ""
	else
		eend 1 "Conversion to gettext failed. Plugin needs fixing."
		return 1
	fi
}

vdr_i18n_disable_gettext() {
	#einfo "Disabling gettext support in plugin"

	# Remove i18n Target if using older vdr
	sed -i Makefile \
		-e '/^all:/s/ i18n//'
}

vdr_i18n() {
	if vdr_has_gettext; then
		#einfo "VDR has gettext support"
		if plugin_has_gettext; then
			#einfo "Plugin has gettext support, fine"
			if [[ ${NO_GETTEXT_HACK} == "1" ]]; then
				ewarn "Please remove unneeded NO_GETTEXT_HACK from ebuild."
			fi
		else
			vdr_i18n_convert_to_gettext
			if [[ $? != 0 ]]; then
				einfo "Plugin has no OSD texts or will have only english OSD texts"
			fi
		fi
	else
		#einfo "VDR has no gettext support"
		if plugin_has_gettext; then
			vdr_i18n_disable_gettext
		fi
	fi
}

vdr-plugin_copy_source_tree() {
	pushd . >/dev/null
	cp -r "${S}" "${T}"/source-tree
	cd "${T}"/source-tree
	cp "${WORKDIR}"/Makefile.before Makefile
	# TODO: Fix this, maybe no longer needed
	sed -i Makefile \
		-e "s:^DVBDIR.*$:DVBDIR = ${DVB_INCLUDE_DIR}:" \
		-e 's:^CXXFLAGS:#CXXFLAGS:' \
		-e 's:-I$(DVBDIR)/include:-I$(DVBDIR):' \
		-e 's:-I$(VDRDIR) -I$(DVBDIR):-I$(DVBDIR) -I$(VDRDIR):'
	popd >/dev/null
}

vdr-plugin_install_source_tree() {
	einfo "Installing sources"
	destdir="${VDRSOURCE_DIR}/vdr-${VDRVERSION}/PLUGINS/src/${VDRPLUGIN}"
	insinto "${destdir}-${PV}"
	doins -r "${T}"/source-tree/*

	dosym "${VDRPLUGIN}-${PV}" "${destdir}"
}

vdr-plugin_print_enable_command() {
	local p_name c=0 l=""
	for p_name in ${vdr_plugin_list}; do
		c=$(( c+1 ))
		l="$l ${p_name#vdr-}"
	done

	elog
	case $c in
	1)	elog "Installed plugin${l}" ;;
	*)	elog "Installed $c plugins:${l}" ;;
	esac
	elog "To activate a plugin execute this command:"
	elog "\teselect vdr-plugin enable <plugin_name> ..."
	elog
}

has_vdr() {
	[[ -f "${VDR_INCLUDE_DIR}"/config.h ]]
}

## exported functions

vdr-plugin_pkg_setup() {
	# -fPIC is needed for shared objects on some platforms (amd64 and others)
	append-flags -fPIC

	# Plugins need to be compiled with position independent code, otherwise linking
	# VDR against it will fail
	if has_version ">=media-video/vdr-1.7.13"; then
		append-lfs-flags
	fi

	# Where should the plugins live in the filesystem
	VDR_PLUGIN_DIR="/usr/$(get_libdir)/vdr/plugins"
	VDR_CHECKSUM_DIR="${VDR_PLUGIN_DIR%/plugins}/checksums"

	# was /usr/lib/... some time ago
	# since gentoo-vdr-scripts-0.3.6 it works with /usr/share/...
	VDR_RC_DIR="/usr/share/vdr/rcscript"

	# Pathes to includes
	VDR_INCLUDE_DIR="/usr/include/vdr"
	DVB_INCLUDE_DIR="/usr/include"

	TMP_LOCALE_DIR="${WORKDIR}/tmp-locale"
	LOCDIR="/usr/share/locale"

	if ! has_vdr; then
		# set to invalid values to detect abuses
		VDRVERSION="eclass_no_vdr_installed"
		APIVERSION="eclass_no_vdr_installed"

		if [[ "${GENTOO_VDR_CONDITIONAL:-no}" = "yes" ]] && ! use vdr; then
			einfo "VDR not found!"
		else
			# if vdr is required
			die "VDR not found!"
		fi
		return
	fi

	VDRVERSION=$(awk -F'"' '/define VDRVERSION/ {print $2}' "${VDR_INCLUDE_DIR}"/config.h)
	APIVERSION=$(awk -F'"' '/define APIVERSION/ {print $2}' "${VDR_INCLUDE_DIR}"/config.h)
	[[ -z ${APIVERSION} ]] && APIVERSION="${VDRVERSION}"

	einfo "Compiling against"
	einfo "\tvdr-${VDRVERSION} [API version ${APIVERSION}]"


	if [ -n "${VDR_LOCAL_PATCHES_DIR}" ]; then
		eerror "Using VDR_LOCAL_PATCHES_DIR is deprecated!"
		eerror "Please move all your patches into"
		eerror "${EROOT}/etc/portage/patches/${CATEGORY}/${P}"
		eerror "and remove or unset the VDR_LOCAL_PATCHES_DIR variable."
		die
	fi
}

vdr-plugin_src_util() {
	while [ "$1" ]; do
		case "$1" in
		all)
			vdr-plugin_src_util unpack add_local_patch patchmakefile i18n
			;;
		prepare|all_but_unpack)
			vdr-plugin_src_util add_local_patch patchmakefile i18n
			;;
		unpack)
			base_src_unpack
			;;
		add_local_patch)
			cd "${S}" || die "Could not change to plugin-source-directory!"
			epatch_user
			;;
		patchmakefile)
			cd "${S}" || die "Could not change to plugin-source-directory!"
			vdr_patchmakefile
			;;
		i18n)
			# i18n-to-gettext tool removed starting with version 1.7.27
			if has_version "<media-video/vdr-1.7.27"; then
				cd "${S}" || die "Could not change to plugin-source-directory!"
				vdr_i18n
			fi
			;;
		esac

		shift
	done
}

vdr-plugin_src_unpack() {
	if [[ -z ${VDR_INCLUDE_DIR} ]]; then
		eerror "Wrong use of vdr-plugin.eclass."
		eerror "An ebuild for a vdr-plugin will not work without calling vdr-plugin_pkg_setup."
		echo
		eerror "Please report this at bugs.gentoo.org."
		die "vdr-plugin_pkg_setup not called!"
	fi
	if [ -z "$1" ]; then
		case "${EAPI:-0}" in
			2|3|4|5)
				vdr-plugin_src_util unpack
				;;
			*)
				vdr-plugin_src_util all
				;;
		esac

	else
		vdr-plugin_src_util $@
	fi
}

vdr-plugin_src_prepare() {
	base_src_prepare
	vdr-plugin_src_util prepare
}

vdr-plugin_src_compile() {
	[ -z "$1" ] && vdr-plugin_src_compile copy_source compile

	while [ "$1" ]; do

		case "$1" in
		copy_source)
			[[ -n "${VDRSOURCE_DIR}" ]] && vdr-plugin_copy_source_tree
			;;
		compile)
			if [[ ! -f ${WORKDIR}/.vdr-plugin_makefile_patched ]]; then
				eerror "Wrong use of vdr-plugin.eclass."
				eerror "An ebuild for a vdr-plugin will not work without"
				eerror "calling vdr-plugin_src_unpack to patch the Makefile."
				echo
				eerror "Please report this at bugs.gentoo.org."
				die "vdr-plugin_src_unpack not called!"
			fi
			cd "${S}"

			BUILD_TARGETS=${BUILD_TARGETS:-${VDRPLUGIN_MAKE_TARGET:-all}}

			emake ${BUILD_PARAMS} \
				${BUILD_TARGETS} \
				LOCALEDIR="${TMP_LOCALE_DIR}" \
				LIBDIR="${S}" \
				TMPDIR="${T}" \
			|| die "emake failed"
			;;
		esac

		shift
	done
}

vdr-plugin_src_install() {
	[[ -n "${VDRSOURCE_DIR}" ]] && vdr-plugin_install_source_tree
	cd "${WORKDIR}"

	if [[ -n ${VDR_MAINTAINER_MODE} ]]; then
		local mname="${P}-Makefile"
		cp "${S}"/Makefile "${mname}.patched"
		cp Makefile.before "${mname}.before"

		diff -u "${mname}.before" "${mname}.patched" > "${mname}.diff"

		insinto "/usr/share/vdr/maintainer-data/makefile-changes"
		doins "${mname}.diff"

		insinto "/usr/share/vdr/maintainer-data/makefile-before"
		doins "${mname}.before"

		insinto "/usr/share/vdr/maintainer-data/makefile-patched"
		doins "${mname}.patched"

	fi



	cd "${S}"
	insinto "${VDR_PLUGIN_DIR}"
	doins libvdr-*.so.*

	# create list of all created plugin libs
	vdr_plugin_list=""
	local p_name
	for p in libvdr-*.so.*; do
		p_name="${p%.so*}"
		p_name="${p_name#lib}"
		vdr_plugin_list="${vdr_plugin_list} ${p_name}"
	done

	create_header_checksum_file ${vdr_plugin_list}
	create_plugindb_file ${vdr_plugin_list}

	if vdr_has_gettext && [[ -d ${TMP_LOCALE_DIR} ]]; then
		einfo "Installing locales"
		cd "${TMP_LOCALE_DIR}"
		insinto "${LOCDIR}"
		doins -r *
	fi

	cd "${S}"
	local docfile
	for docfile in README* HISTORY CHANGELOG; do
		[[ -f ${docfile} ]] && dodoc ${docfile}
	done

	# if VDR_CONFD_FILE is empty and ${FILESDIR}/confd exists take it
	[[ -z ${VDR_CONFD_FILE} ]] && [[ -e ${FILESDIR}/confd ]] && VDR_CONFD_FILE=${FILESDIR}/confd

	if [[ -n ${VDR_CONFD_FILE} ]]; then
		newconfd "${VDR_CONFD_FILE}" vdr.${VDRPLUGIN}
	fi


	# if VDR_RCADDON_FILE is empty and ${FILESDIR}/rc-addon.sh exists take it
	[[ -z ${VDR_RCADDON_FILE} ]] && [[ -e ${FILESDIR}/rc-addon.sh ]] && VDR_RCADDON_FILE=${FILESDIR}/rc-addon.sh

	if [[ -n ${VDR_RCADDON_FILE} ]]; then
		insinto "${VDR_RC_DIR}"
		newins "${VDR_RCADDON_FILE}" plugin-${VDRPLUGIN}.sh
	fi
}

vdr-plugin_pkg_postinst() {
	vdr-plugin_print_enable_command

	if [[ -n "${VDR_CONFD_FILE}" ]]; then
		elog "Please have a look at the config-file"
		elog "\t/etc/conf.d/vdr.${VDRPLUGIN}"
		elog
	fi
}

vdr-plugin_pkg_postrm() {
	delete_orphan_plugindb_file
}

vdr-plugin_pkg_config() {
:
}

case "${EAPI:-0}" in
	2|3|4|5)
		EXPORT_FUNCTIONS pkg_setup src_unpack src_prepare src_compile src_install pkg_postinst pkg_postrm pkg_config
		;;
	*)
		EXPORT_FUNCTIONS pkg_setup src_unpack src_compile src_install pkg_postinst pkg_postrm pkg_config
		;;
esac
